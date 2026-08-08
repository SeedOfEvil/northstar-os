#include "packagecatalog.h"

#include <QDateTime>
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
    return toVariantList(filterPackages(m_packages, m_query));
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
    emit statusChanged();

    QProcess process;
    process.setProgram(m_packageManagerPath);
    process.setArguments({QStringLiteral("query"), QStringLiteral("-a"),
                          QStringLiteral("%n|%v|%c")});
    process.start();

    if (!process.waitForStarted(1500)) {
        m_refreshing = false;
        setStatusMessage(QStringLiteral("Unable to start FreeBSD pkg."));
        return false;
    }
    if (!process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished(1000);
        m_refreshing = false;
        setStatusMessage(QStringLiteral("FreeBSD pkg did not finish the inventory query."));
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        m_refreshing = false;
        setStatusMessage(QStringLiteral("FreeBSD pkg could not read the installed package inventory."));
        return false;
    }

    m_packages = parseQueryOutput(process.readAllStandardOutput());
    emit packagesChanged();
    emit matchingPackagesChanged();
    m_lastRefresh = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_refreshing = false;
    setStatusMessage(m_packages.isEmpty()
            ? QStringLiteral("No installed packages were reported by FreeBSD pkg.")
            : QStringLiteral("%1 installed package%2 loaded.")
                  .arg(m_packages.size())
                  .arg(m_packages.size() == 1 ? QString() : QStringLiteral("s")));
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

        InstalledPackage package{
            boundedField(line.left(firstSeparator), 128),
            boundedField(line.mid(firstSeparator + 1, secondSeparator - firstSeparator - 1), 128),
            boundedField(line.mid(secondSeparator + 1), 240),
        };
        if (package.name.isEmpty() || package.version.isEmpty() || seenNames.contains(package.name)) {
            continue;
        }
        seenNames.insert(package.name);
        result.append(std::move(package));
    }

    std::sort(result.begin(), result.end(), packageLessThan);
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
