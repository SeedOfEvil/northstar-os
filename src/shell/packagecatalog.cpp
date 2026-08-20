#include "packagecatalog.h"

#include <QDateTime>
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
                          QStringLiteral("%n|%v|%c|%a")});
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

        // A comment may itself contain a separator, so the fields cannot be
        // split apart wholesale. The automatic flag is the last field and is
        // only ever 0 or 1, which is what makes it safe to recognise from the
        // end of the line, and what keeps a comment ending in a separator
        // from being mistaken for one.
        qsizetype commentEnd = line.size();
        bool automatic = false;
        const qsizetype lastSeparator = line.lastIndexOf(QLatin1Char('|'));
        if (lastSeparator > secondSeparator) {
            const QString trailing = line.mid(lastSeparator + 1).trimmed();
            if (trailing == QLatin1String("0") || trailing == QLatin1String("1")) {
                commentEnd = lastSeparator;
                automatic = trailing == QLatin1String("1");
            }
        }

        InstalledPackage package;
        package.name = boundedField(line.left(firstSeparator), 128);
        package.version =
            boundedField(line.mid(firstSeparator + 1, secondSeparator - firstSeparator - 1), 128);
        package.comment =
            boundedField(line.mid(secondSeparator + 1, commentEnd - secondSeparator - 1), 240);
        package.automatic = automatic;
        if (package.name.isEmpty() || package.version.isEmpty() || seenNames.contains(package.name)) {
            continue;
        }
        seenNames.insert(package.name);
        result.append(std::move(package));
    }

    std::sort(result.begin(), result.end(), packageLessThan);
    return result;
}

QStringList PackageCatalog::filters()
{
    return QStringList{requestedFilter(), updatableFilter(), allFilter()};
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
        const QString searchText = QStringList{package.name, package.version, package.comment}
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
