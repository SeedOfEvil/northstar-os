#include "packagecatalog.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QHash>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace {

QString boundedField(const QString &value, qsizetype maximumLength)
{
    QString result = value.trimmed();
    result.replace(QLatin1Char('\n'), QLatin1Char(' '));
    result.replace(QLatin1Char('\r'), QLatin1Char(' '));
    if (result.size() > maximumLength) {
        result.truncate(maximumLength);
    }
    return result;
}

bool packageLessThan(const InstalledPackage &left, const InstalledPackage &right)
{
    const int nameComparison = QString::compare(left.name, right.name, Qt::CaseInsensitive);
    if (nameComparison != 0) {
        return nameComparison < 0;
    }
    return QString::compare(left.version, right.version, Qt::CaseInsensitive) < 0;
}

bool canonicalPackageLessThan(const InstalledPackage &left, const InstalledPackage &right)
{
    const QString leftIdentity = left.name + QLatin1Char('|') + left.version
        + QLatin1Char('|') + left.origin;
    const QString rightIdentity = right.name + QLatin1Char('|') + right.version
        + QLatin1Char('|') + right.origin;
    return QString::compare(leftIdentity, rightIdentity, Qt::CaseSensitive) < 0;
}

QByteArray canonicalCatalogue(const QList<InstalledPackage> &packages)
{
    QByteArray canonical;
    for (const InstalledPackage &package : packages) {
        canonical += package.name.toUtf8();
        canonical += '|';
        canonical += package.version.toUtf8();
        canonical += '|';
        canonical += package.origin.toUtf8();
        canonical += '\n';
    }
    return canonical;
}

} // namespace

PackageCatalog::PackageCatalog(QString packageManagerPath, QObject *parent)
    : QObject(parent)
    , m_filter(requestedFilter())
    , m_packageManagerPath(packageManagerPath.trimmed())
{
    if (m_packageManagerPath.isEmpty()) {
        m_packageManagerPath = QStandardPaths::findExecutable(QStringLiteral("pkg"));
    }
    setStatusMessage(m_packageManagerPath.isEmpty()
            ? QStringLiteral("FreeBSD pkg is unavailable on this host.")
            : QStringLiteral("Package inventory has not been refreshed yet."));
}

QVariantList PackageCatalog::packages() const
{
    return toVariantList(m_packages);
}

QVariantList PackageCatalog::matchingPackages() const
{
    return toVariantList(filterPackages(visiblePackages(), m_query));
}

QString PackageCatalog::query() const
{
    return m_query;
}

bool PackageCatalog::available() const
{
    return !m_packageManagerPath.isEmpty();
}

bool PackageCatalog::refreshing() const
{
    return m_refreshing;
}

QString PackageCatalog::statusMessage() const
{
    return m_statusMessage;
}

QString PackageCatalog::packageManagerPath() const
{
    return m_packageManagerPath;
}

int PackageCatalog::installedCount() const
{
    return m_packages.size();
}

QString PackageCatalog::lastRefresh() const
{
    return m_lastRefresh;
}

bool PackageCatalog::refresh()
{
    if (m_refreshing) {
        return false;
    }
    if (!available()) {
        setStatusMessage(QStringLiteral("FreeBSD pkg is unavailable on this host."));
        return false;
    }

    m_refreshing = true;
    emit refreshingChanged();

    QProcess process;
    process.setProgram(m_packageManagerPath);
    process.setArguments({QStringLiteral("query"), QStringLiteral("-a"),
                          QStringLiteral("%n|%v|%o|%R|%a|%k|%c")});
    process.start();

    if (!process.waitForStarted(1500)) {
        m_refreshing = false;
        emit refreshingChanged();
        setStatusMessage(QStringLiteral("Unable to start FreeBSD pkg."));
        return false;
    }
    if (!process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished(1000);
        m_refreshing = false;
        emit refreshingChanged();
        setStatusMessage(QStringLiteral("FreeBSD pkg did not finish the inventory query."));
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        m_refreshing = false;
        emit refreshingChanged();
        setStatusMessage(QStringLiteral("FreeBSD pkg could not read the installed package inventory."));
        return false;
    }

    m_packages = parseQueryOutput(process.readAllStandardOutput());
    assignRemovalIndexes();
    emit packagesChanged();
    emit matchingPackagesChanged();
    m_lastRefresh = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_refreshing = false;
    emit refreshingChanged();
    setStatusMessage(m_packages.isEmpty()
            ? QStringLiteral("No installed packages were reported by FreeBSD pkg.")
            : QStringLiteral("%1 requested, %2 installed as dependencies.")
                  .arg(requestedCount())
                  .arg(dependencyCount()));
    return true;
}

QList<InstalledPackage> PackageCatalog::parseQueryOutput(const QByteArray &output)
{
    QList<InstalledPackage> result;
    QSet<QString> seenNames;
    const QStringList lines = QString::fromUtf8(output).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const qsizetype firstSeparator = line.indexOf(QLatin1Char('|'));
        const qsizetype secondSeparator = firstSeparator < 0
            ? -1 : line.indexOf(QLatin1Char('|'), firstSeparator + 1);
        if (firstSeparator <= 0 || secondSeparator <= firstSeparator + 1) {
            continue;
        }

        // New inventory rows put fixed provenance and automatic fields before
        // the comment so a comment may contain any number of separators. Keep
        // accepting the older name|version|comment|automatic test fixture.
        qsizetype commentEnd = line.size();
        bool automatic = false;
        qsizetype commentStart = secondSeparator + 1;
        QString origin;
        QString repository;
        const qsizetype thirdSeparator = line.indexOf(QLatin1Char('|'), secondSeparator + 1);
        const qsizetype fourthSeparator = thirdSeparator < 0
            ? -1 : line.indexOf(QLatin1Char('|'), thirdSeparator + 1);
        const qsizetype fifthSeparator = fourthSeparator < 0
            ? -1 : line.indexOf(QLatin1Char('|'), fourthSeparator + 1);
        const qsizetype sixthSeparator = fifthSeparator < 0
            ? -1 : line.indexOf(QLatin1Char('|'), fifthSeparator + 1);
        bool locked = false;
        if (thirdSeparator > secondSeparator && fourthSeparator > thirdSeparator
            && fifthSeparator > fourthSeparator && sixthSeparator > fifthSeparator) {
            const QString flag = line.mid(fourthSeparator + 1,
                                          fifthSeparator - fourthSeparator - 1).trimmed();
            const QString lockedFlag = line.mid(fifthSeparator + 1,
                                                sixthSeparator - fifthSeparator - 1).trimmed();
            if ((flag == QLatin1String("0") || flag == QLatin1String("1"))
                && (lockedFlag == QLatin1String("0") || lockedFlag == QLatin1String("1"))) {
                origin = boundedField(line.mid(secondSeparator + 1,
                                               thirdSeparator - secondSeparator - 1), 192);
                repository = boundedField(line.mid(thirdSeparator + 1,
                                                   fourthSeparator - thirdSeparator - 1), 128);
                automatic = flag == QLatin1String("1");
                locked = lockedFlag == QLatin1String("1");
                commentStart = sixthSeparator + 1;
            }
        } else {
            const qsizetype lastSeparator = line.lastIndexOf(QLatin1Char('|'));
            if (lastSeparator > secondSeparator) {
                const QString trailing = line.mid(lastSeparator + 1).trimmed();
                if (trailing == QLatin1String("0") || trailing == QLatin1String("1")) {
                    commentEnd = lastSeparator;
                    automatic = trailing == QLatin1String("1");
                }
            }
        }

        InstalledPackage package;
        package.name = boundedField(line.left(firstSeparator), 128);
        package.version =
            boundedField(line.mid(firstSeparator + 1, secondSeparator - firstSeparator - 1), 128);
        package.comment = boundedField(line.mid(commentStart, commentEnd - commentStart), 240);
        package.origin = origin;
        package.repository = repository;
        package.automatic = automatic;
        package.locked = locked;
        if (package.name.isEmpty() || package.version.isEmpty() || seenNames.contains(package.name)) {
            continue;
        }
        seenNames.insert(package.name);
        result.append(std::move(package));
    }

    std::sort(result.begin(), result.end(), canonicalPackageLessThan);
    return result;
}

QList<InstalledPackage> PackageCatalog::parseRemoteQueryOutput(const QByteArray &output,
                                                                const QString &repository)
{
    QList<InstalledPackage> result;
    QSet<QString> seen;
    const QStringList lines = QString::fromUtf8(output).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const qsizetype first = line.indexOf(QLatin1Char('|'));
        const qsizetype second = first < 0 ? -1 : line.indexOf(QLatin1Char('|'), first + 1);
        const qsizetype third = second < 0 ? -1 : line.indexOf(QLatin1Char('|'), second + 1);
        if (first <= 0 || second <= first + 1 || third <= second + 1) {
            continue;
        }
        InstalledPackage package;
        package.name = boundedField(line.left(first), 128);
        package.version = boundedField(line.mid(first + 1, second - first - 1), 128);
        package.origin = boundedField(line.mid(second + 1, third - second - 1), 192);
        package.comment = boundedField(line.mid(third + 1), 240);
        package.repository = repository;
        package.installed = false;
        const QString identity = package.name + QLatin1Char('|') + package.version
            + QLatin1Char('|') + package.origin;
        if (package.name.isEmpty() || package.version.isEmpty() || package.origin.isEmpty()
            || seen.contains(identity)) {
            continue;
        }
        seen.insert(identity);
        result.append(std::move(package));
    }
    std::sort(result.begin(), result.end(), packageLessThan);
    return result;
}

QStringList PackageCatalog::filters()
{
    return QStringList{requestedFilter(), availableFilter(), updatableFilter(), allFilter()};
}

QString PackageCatalog::requestedFilter()
{
    return QStringLiteral("requested");
}

QString PackageCatalog::updatableFilter()
{
    return QStringLiteral("updatable");
}

QString PackageCatalog::allFilter()
{
    return QStringLiteral("all");
}

QString PackageCatalog::availableFilter()
{
    return QStringLiteral("available");
}

QString PackageCatalog::filter() const
{
    return m_filter;
}

void PackageCatalog::setFilter(const QString &filter)
{
    const QString requested = filter.trimmed().toLower();
    if (!filters().contains(requested) || m_filter == requested) {
        return;
    }
    m_filter = requested;
    emit filterChanged();
    emit matchingPackagesChanged();
}

int PackageCatalog::requestedCount() const
{
    return static_cast<int>(std::count_if(m_packages.cbegin(), m_packages.cend(),
                                          [](const InstalledPackage &package) {
        return !package.automatic;
    }));
}

int PackageCatalog::dependencyCount() const
{
    return static_cast<int>(m_packages.size()) - requestedCount();
}

int PackageCatalog::updatableCount() const
{
    return static_cast<int>(std::count_if(m_packages.cbegin(), m_packages.cend(),
                                          [](const InstalledPackage &package) {
        return package.updatable;
    }));
}

bool PackageCatalog::scanningUpdates() const
{
    return m_updateScan != nullptr;
}

bool PackageCatalog::updatesKnown() const
{
    return m_updatesKnown;
}

QString PackageCatalog::updateStatus() const
{
    return m_updateStatus;
}

QList<InstalledPackage> PackageCatalog::visiblePackages() const
{
    if (m_filter == availableFilter()) {
        return m_availablePackages;
    }
    if (m_filter == allFilter()) {
        return m_packages;
    }

    QList<InstalledPackage> visible;
    for (const InstalledPackage &package : m_packages) {
        if (m_filter == updatableFilter() ? package.updatable : !package.automatic) {
            visible.append(package);
        }
    }
    return visible;
}

bool PackageCatalog::refreshingAvailable() const
{
    return m_availableScan != nullptr;
}

bool PackageCatalog::availableCatalogReady() const
{
    return !m_catalogueDigest.isEmpty();
}

int PackageCatalog::availableCount() const
{
    return m_availablePackages.size();
}

QString PackageCatalog::availableStatus() const
{
    return m_availableStatus;
}

QString PackageCatalog::repositoryName() const
{
    return m_repositoryName;
}

QString PackageCatalog::catalogueDigest() const
{
    return m_catalogueDigest;
}

bool PackageCatalog::refreshAvailable()
{
    if (m_packageManagerPath.isEmpty() || m_availableScan) {
        return false;
    }
    m_availableScan = new QProcess(this);
    m_availableScan->setProgram(m_packageManagerPath);
    m_availableScan->setArguments({QStringLiteral("rquery"), QStringLiteral("-r"),
                                   m_repositoryName, QStringLiteral("%n|%v|%o|%c")});
    m_availableScan->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_availableScan, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        const bool succeeded = exitStatus == QProcess::NormalExit && exitCode == 0;
        const QByteArray output = succeeded ? m_availableScan->readAllStandardOutput() : QByteArray();
        m_availableScan->deleteLater();
        m_availableScan = nullptr;
        if (succeeded) {
            applyAvailableCatalog(output);
        } else {
            m_catalogueDigest.clear();
            m_availablePackages.clear();
            m_availableStatus = QStringLiteral("The pinned FreeBSD package catalogue is unavailable.");
            emit availableCatalogChanged();
            emit matchingPackagesChanged();
        }
    });
    connect(m_availableScan, &QProcess::errorOccurred, this, [this]() {
        if (!m_availableScan) {
            return;
        }
        m_availableScan->deleteLater();
        m_availableScan = nullptr;
        m_catalogueDigest.clear();
        m_availablePackages.clear();
        m_availableStatus = QStringLiteral("Unable to read the pinned FreeBSD package catalogue.");
        emit availableCatalogChanged();
        emit matchingPackagesChanged();
    });
    m_availableStatus = QStringLiteral("Reading the pinned FreeBSD package catalogue...");
    m_availableScan->start(QIODevice::ReadOnly);
    emit availableCatalogChanged();
    return true;
}

void PackageCatalog::applyAvailableCatalog(const QByteArray &output)
{
    QList<InstalledPackage> all = parseRemoteQueryOutput(output, m_repositoryName);
    for (qsizetype index = 0; index < all.size(); ++index) {
        all[index].planIndex = static_cast<int>(index);
    }
    m_catalogueDigest = QString::fromLatin1(
        QCryptographicHash::hash(canonicalCatalogue(all), QCryptographicHash::Sha256).toHex());
    QSet<QString> installedNames;
    for (const InstalledPackage &package : m_packages) {
        installedNames.insert(package.name);
    }
    m_availablePackages.clear();
    for (const InstalledPackage &package : std::as_const(all)) {
        if (!installedNames.contains(package.name) && package.name != QLatin1String("pkg")) {
            m_availablePackages.append(package);
        }
    }
    m_availableStatus = QStringLiteral("%1 packages are available from authenticated source %2.")
                            .arg(m_availablePackages.size())
                            .arg(m_repositoryName);
    emit availableCatalogChanged();
    emit matchingPackagesChanged();
}

void PackageCatalog::assignRemovalIndexes()
{
    QList<qsizetype> removable;
    for (qsizetype index = 0; index < m_packages.size(); ++index) {
        InstalledPackage &package = m_packages[index];
        package.planIndex = -1;
        if (!package.automatic && !package.locked && package.repository == m_repositoryName
            && package.name != QLatin1String("pkg") && !package.origin.isEmpty()) {
            removable.append(index);
        }
    }
    std::sort(removable.begin(), removable.end(), [this](qsizetype left, qsizetype right) {
        return canonicalPackageLessThan(m_packages.at(left), m_packages.at(right));
    });
    for (qsizetype planIndex = 0; planIndex < removable.size(); ++planIndex) {
        m_packages[removable.at(planIndex)].planIndex = static_cast<int>(planIndex);
    }
}

bool PackageCatalog::scanForUpdates()
{
    if (m_packageManagerPath.isEmpty() || m_updateScan) {
        return false;
    }

    // Reading the inventory takes milliseconds; comparing it against the
    // repository catalogue takes seconds, because that is a different and far
    // larger question. Waiting for it here would freeze every shell surface,
    // so it runs on its own and the answer arrives when it arrives.
    m_updateScan = new QProcess(this);
    m_updateScan->setProgram(m_packageManagerPath);
    m_updateScan->setArguments({QStringLiteral("version"), QStringLiteral("-vRL=")});
    connect(m_updateScan, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        QByteArray output;
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            output = m_updateScan->readAllStandardOutput();
        }
        const bool succeeded = exitStatus == QProcess::NormalExit && exitCode == 0;
        m_updateScan->deleteLater();
        m_updateScan = nullptr;

        if (!succeeded) {
            m_updatesKnown = false;
            m_updateStatus =
                QStringLiteral("Unable to check for updates. The repository catalogue may not "
                               "have been fetched yet.");
            emit updateScanChanged();
            return;
        }

        applyUpdateScan(output);
    });
    connect(m_updateScan, &QProcess::errorOccurred, this, [this]() {
        if (!m_updateScan) {
            return;
        }
        m_updateScan->deleteLater();
        m_updateScan = nullptr;
        m_updatesKnown = false;
        m_updateStatus = QStringLiteral("Unable to run the update check.");
        emit updateScanChanged();
    });

    m_updateStatus = QStringLiteral("Checking for updates...");
    m_updateScan->start(QIODevice::ReadOnly);
    emit updateScanChanged();
    return true;
}

void PackageCatalog::applyUpdateScan(const QByteArray &output)
{
    const QList<InstalledPackage> scanned = parseVersionOutput(output);

    QHash<QString, const InstalledPackage *> byName;
    for (const InstalledPackage &package : scanned) {
        byName.insert(package.name, &package);
    }

    for (InstalledPackage &package : m_packages) {
        const auto match = byName.constFind(package.name);
        package.updatable = match != byName.constEnd() && (*match)->updatable;
        package.orphaned = match != byName.constEnd() && (*match)->orphaned;
        package.availableVersion =
            match != byName.constEnd() ? (*match)->availableVersion : QString();
    }

    m_updatesKnown = true;
    const int count = updatableCount();
    m_updateStatus = count == 0
        ? QStringLiteral("Everything is up to date.")
        : QStringLiteral("%1 package%2 can be updated.")
              .arg(count)
              .arg(count == 1 ? QString() : QStringLiteral("s"));

    emit packagesChanged();
    emit matchingPackagesChanged();
    emit updateScanChanged();
}

// pkg version -vRL= prints one line per package that is not current:
//
//   firefox-153.0.1,2   <   needs updating (remote has 153.0.3,2)
//   northstar-0.1.4     ?   orphaned: x11/northstar
//
// The name and installed version are joined by the last hyphen, and the
// character in the middle says which case this is.
QList<InstalledPackage> PackageCatalog::parseVersionOutput(const QByteArray &output)
{
    QList<InstalledPackage> result;
    const QStringList lines = QString::fromUtf8(output).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        const QStringList fields = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() < 2) {
            continue;
        }

        const QString nameAndVersion = fields.at(0);
        const qsizetype separator = nameAndVersion.lastIndexOf(QLatin1Char('-'));
        if (separator <= 0) {
            continue;
        }

        InstalledPackage package;
        package.name = boundedField(nameAndVersion.left(separator), 128);
        package.version = boundedField(nameAndVersion.mid(separator + 1), 128);
        if (package.name.isEmpty()) {
            continue;
        }

        const QString state = fields.at(1);
        if (state == QLatin1String("<")) {
            package.updatable = true;
            // The remote version is the last word of "(remote has X)".
            const qsizetype remoteAt = trimmed.indexOf(QLatin1String("remote has "));
            if (remoteAt >= 0) {
                QString remote = trimmed.mid(remoteAt + 11).trimmed();
                if (remote.endsWith(QLatin1Char(')'))) {
                    remote.chop(1);
                }
                package.availableVersion = boundedField(remote, 128);
            }
        } else if (state == QLatin1String("?")) {
            // Installed, but its origin is no longer in the ports tree, so
            // there is nothing to update it to.
            package.orphaned = true;
        } else {
            continue;
        }

        result.append(package);
    }

    return result;
}

QList<InstalledPackage> PackageCatalog::filterPackages(const QList<InstalledPackage> &packages,
                                                        const QString &query)
{
    const QStringList terms = query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (terms.isEmpty()) {
        return packages;
    }

    QList<InstalledPackage> matches;
    for (const InstalledPackage &package : packages) {
        const QString searchText = QStringList{package.name, package.version, package.comment,
                                               package.origin, package.repository}
            .join(QLatin1Char(' '));
        const bool matchesAllTerms = std::all_of(terms.cbegin(), terms.cend(), [&searchText](const QString &term) {
            return searchText.contains(term, Qt::CaseInsensitive);
        });
        if (matchesAllTerms) {
            matches.append(package);
        }
    }
    return matches;
}

QVariantList PackageCatalog::toVariantList(const QList<InstalledPackage> &packages)
{
    QVariantList result;
    result.reserve(packages.size());
    for (const InstalledPackage &package : packages) {
        result.append(QVariantMap{
            {QStringLiteral("name"), package.name},
            {QStringLiteral("version"), package.version},
            {QStringLiteral("comment"), package.comment},
            {QStringLiteral("origin"), package.origin},
            {QStringLiteral("repository"), package.repository},
            {QStringLiteral("installed"), package.installed},
            {QStringLiteral("locked"), package.locked},
            {QStringLiteral("planIndex"), package.planIndex},
            {QStringLiteral("automatic"), package.automatic},
            {QStringLiteral("updatable"), package.updatable},
            {QStringLiteral("availableVersion"), package.availableVersion},
            {QStringLiteral("orphaned"), package.orphaned},
        });
    }
    return result;
}

void PackageCatalog::setQuery(const QString &query)
{
    const QString normalizedQuery = query.simplified();
    if (m_query == normalizedQuery) {
        return;
    }
    m_query = normalizedQuery;
    emit queryChanged();
    emit matchingPackagesChanged();
}

void PackageCatalog::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusChanged();
}
