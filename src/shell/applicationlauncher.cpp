#include "applicationlauncher.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace {

QString defaultLaunchLogPath()
{
    QString stateHome = qEnvironmentVariable("XDG_STATE_HOME");
    if (stateHome.isEmpty()) {
        stateHome = QDir::home().filePath(QStringLiteral(".local/state"));
    }
    return QDir(stateHome).filePath(QStringLiteral("northstar/launch.log"));
}

QString defaultAssociationSettingsPath()
{
    QString configHome = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configHome.isEmpty()) {
        configHome = QDir::home().filePath(QStringLiteral(".config/northstar"));
    }
    return QDir(configHome).filePath(QStringLiteral("file-associations.ini"));
}

QString logValue(const QString &value)
{
    QString result = value;
    result.replace(QLatin1Char('\n'), QLatin1Char(' '));
    result.replace(QLatin1Char('\r'), QLatin1Char(' '));
    result.replace(QLatin1Char('='), QLatin1Char('_'));
    return result;
}

QVariantList mergeApplicationLists(const QVariantList &first, const QVariantList &second)
{
    QVariantList merged = first;
    merged.append(second);
    std::sort(merged.begin(), merged.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap leftMap = left.toMap();
        const QVariantMap rightMap = right.toMap();
        const int nameComparison = QString::compare(
            leftMap.value(QStringLiteral("name")).toString(),
            rightMap.value(QStringLiteral("name")).toString(),
            Qt::CaseInsensitive);
        if (nameComparison != 0) {
            return nameComparison < 0;
        }
        return QString::compare(
                   leftMap.value(QStringLiteral("desktopId")).toString(),
                   rightMap.value(QStringLiteral("desktopId")).toString(),
                   Qt::CaseInsensitive)
            < 0;
    });
    return merged;
}

} // namespace

ApplicationLauncher::ApplicationLauncher(
    QObject *parent,
    LaunchFunction launchFunction,
    QStringList applicationDirectories,
    QString launchLogPath,
    QStringList bundleDirectories,
    QString associationSettingsPath)
    : QObject(parent)
    , m_catalog(std::move(applicationDirectories))
    , m_bundleCatalog(std::move(bundleDirectories))
    , m_launchFunction(std::move(launchFunction))
    , m_launchLogPath(launchLogPath.isEmpty() ? defaultLaunchLogPath() : std::move(launchLogPath))
    , m_associationSettingsPath(associationSettingsPath.trimmed().isEmpty()
            ? defaultAssociationSettingsPath()
            : QDir::cleanPath(QDir::fromNativeSeparators(associationSettingsPath)))
{
    if (!m_launchFunction) {
        m_launchFunction = [](const QString &program, const QStringList &arguments, qint64 *pid) {
            return QProcess::startDetached(program, arguments, QString(), pid);
        };
    }

    connect(&m_catalog, &ApplicationCatalog::applicationsChanged, this, &ApplicationLauncher::applicationsChanged);
    connect(&m_catalog, &ApplicationCatalog::applicationsChanged, this, &ApplicationLauncher::matchingApplicationsChanged);
    connect(&m_bundleCatalog, &ApplicationBundleCatalog::applicationsChanged, this, &ApplicationLauncher::applicationsChanged);
    connect(&m_bundleCatalog, &ApplicationBundleCatalog::applicationsChanged, this, &ApplicationLauncher::matchingApplicationsChanged);
}

QVariantList ApplicationLauncher::applications() const
{
    return mergeApplicationLists(m_catalog.applications(), m_bundleCatalog.applications());
}

QString ApplicationLauncher::applicationQuery() const
{
    return m_applicationQuery;
}

QVariantList ApplicationLauncher::matchingApplications() const
{
    return mergeApplicationLists(
        m_catalog.searchApplications(m_applicationQuery),
        m_bundleCatalog.searchApplications(m_applicationQuery));
}

QString ApplicationLauncher::launchMessage() const
{
    return m_launchMessage;
}

QString ApplicationLauncher::lastLaunchDesktopId() const
{
    return m_lastLaunchDesktopId;
}

QString ApplicationLauncher::lastLaunchProgram() const
{
    return m_lastLaunchProgram;
}

qint64 ApplicationLauncher::lastLaunchPid() const
{
    return m_lastLaunchPid;
}

bool ApplicationLauncher::lastLaunchSucceeded() const
{
    return m_lastLaunchSucceeded;
}

QString ApplicationLauncher::launchLogPath() const
{
    return m_launchLogPath;
}

bool ApplicationLauncher::launchTerminal()
{
    return launch(QStringLiteral("qterminal"), QStringLiteral("Terminal"), QStringLiteral("qterminal"), {});
}

bool ApplicationLauncher::launchBrowser()
{
    return launch(QStringLiteral("firefox"), QStringLiteral("Firefox"), QStringLiteral("firefox"), {});
}

bool ApplicationLauncher::launchApplication(const QString &desktopId)
{
    QString program;
    QStringList arguments;
    if (!launchSpec(desktopId, &program, &arguments)) {
        setLaunchStatus(desktopId, applicationNameFor(desktopId), {}, 0, false);
        return false;
    }

    return launch(desktopId, applicationNameFor(desktopId), program, arguments);
}

bool ApplicationLauncher::launchApplicationWithFile(const QString &desktopId, const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        setLaunchStatus(desktopId, applicationNameFor(desktopId), {}, 0, false);
        return false;
    }

    QString program;
    QStringList arguments;
    if (!launchSpec(desktopId, &program, &arguments)) {
        setLaunchStatus(desktopId, applicationNameFor(desktopId), {}, 0, false);
        return false;
    }

    arguments.append(fileInfo.absoluteFilePath());
    return launch(desktopId, applicationNameFor(desktopId), program, arguments);
}

QString ApplicationLauncher::preferredApplicationForFile(const QString &filePath) const
{
    const QString extension = associationExtension(filePath);
    if (extension.isEmpty()) {
        return {};
    }

    QSettings settings(m_associationSettingsPath, QSettings::IniFormat);
    const QString desktopId = settings.value(
        QStringLiteral("fileAssociations/%1").arg(extension)).toString().trimmed();
    if (desktopId.isEmpty() || !applicationIds().contains(desktopId)) {
        return {};
    }
    return desktopId;
}

bool ApplicationLauncher::setPreferredApplicationForFile(const QString &filePath,
                                                          const QString &desktopId)
{
    const QString extension = associationExtension(filePath);
    const QString normalizedDesktopId = desktopId.trimmed();
    if (extension.isEmpty() || normalizedDesktopId.isEmpty()
        || !applicationIds().contains(normalizedDesktopId)
        || !ensureAssociationSettingsDirectory()) {
        return false;
    }

    QSettings settings(m_associationSettingsPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("fileAssociations/%1").arg(extension), normalizedDesktopId);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    emit fileAssociationsChanged();
    return true;
}

bool ApplicationLauncher::clearPreferredApplicationForFile(const QString &filePath)
{
    const QString extension = associationExtension(filePath);
    if (extension.isEmpty()) {
        return false;
    }

    QSettings settings(m_associationSettingsPath, QSettings::IniFormat);
    settings.remove(QStringLiteral("fileAssociations/%1").arg(extension));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    emit fileAssociationsChanged();
    return true;
}

bool ApplicationLauncher::refreshApplications()
{
    const bool desktopChanged = m_catalog.reload();
    const bool bundleChanged = m_bundleCatalog.reload();
    return desktopChanged || bundleChanged;
}

void ApplicationLauncher::setApplicationQuery(const QString &query)
{
    if (m_applicationQuery == query) {
        return;
    }

    m_applicationQuery = query;
    emit applicationQueryChanged();
    emit matchingApplicationsChanged();
}

void ApplicationLauncher::clearLaunchMessage()
{
    if (m_launchMessage.isEmpty()) {
        return;
    }

    m_launchMessage.clear();
    emit launchStatusChanged();
}

bool ApplicationLauncher::launch(const QString &desktopId,
                                 const QString &applicationName,
                                 const QString &program,
                                 const QStringList &arguments)
{
    qint64 pid = 0;
    const bool succeeded = m_launchFunction(program, arguments, &pid);
    setLaunchStatus(desktopId, applicationName, program, succeeded ? pid : 0, succeeded);
    return succeeded;
}

void ApplicationLauncher::recordLaunch(const QString &desktopId,
                                       const QString &applicationName,
                                       const QString &program,
                                       qint64 pid,
                                       bool succeeded)
{
    const QFileInfo logInfo(m_launchLogPath);
    const QString directoryPath = logInfo.absolutePath();
    if (!QDir().mkpath(directoryPath)) {
        return;
    }
    QFile::setPermissions(directoryPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    QFile logFile(m_launchLogPath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    logFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const QString result = succeeded ? QStringLiteral("started") : QStringLiteral("failed");
    const QString line = QStringLiteral("timestamp=%1 desktop_id=%2 application=%3 program=%4 pid=%5 result=%6\n")
        .arg(logValue(timestamp),
             logValue(desktopId),
             logValue(applicationName),
             logValue(program),
             QString::number(pid),
             result);
    logFile.write(line.toUtf8());
}

void ApplicationLauncher::setLaunchStatus(const QString &desktopId,
                                           const QString &applicationName,
                                           const QString &program,
                                           qint64 pid,
                                           bool succeeded)
{
    m_lastLaunchDesktopId = desktopId;
    m_lastLaunchProgram = program;
    m_lastLaunchPid = pid;
    m_lastLaunchSucceeded = succeeded;

    if (succeeded) {
        const QString pidSuffix = pid > 0 ? QStringLiteral(" (PID %1)").arg(pid) : QString();
        m_launchMessage = QStringLiteral("Started %1%2").arg(applicationName, pidSuffix);
    } else {
        m_launchMessage = QStringLiteral("Could not start %1").arg(applicationName);
    }

    recordLaunch(desktopId, applicationName, program, pid, succeeded);
    emit launchStatusChanged();
}

QString ApplicationLauncher::applicationNameFor(const QString &desktopId) const
{
    for (const DesktopApplication &application : m_catalog.entries()) {
        if (application.desktopId == desktopId) {
            return application.name;
        }
    }
    for (const BundleApplication &application : m_bundleCatalog.entries()) {
        if (application.desktopId == desktopId) {
            return application.name;
        }
    }
    return desktopId;
}

bool ApplicationLauncher::launchSpec(const QString &desktopId,
                                     QString *program,
                                     QStringList *arguments) const
{
    return m_catalog.launchSpec(desktopId, program, arguments)
        || m_bundleCatalog.launchSpec(desktopId, program, arguments);
}

QStringList ApplicationLauncher::applicationIds() const
{
    QStringList result = m_catalog.applicationIds();
    for (const QString &desktopId : m_bundleCatalog.applicationIds()) {
        if (!result.contains(desktopId)) {
            result.append(desktopId);
        }
    }
    return result;
}

QString ApplicationLauncher::associationExtension(const QString &filePath)
{
    const QString extension = QFileInfo(filePath).suffix().trimmed().toLower();
    if (extension.isEmpty() || extension.size() > 32) {
        return {};
    }
    for (const QChar character : extension) {
        if (!(character.isLetterOrNumber() || character == QLatin1Char('-')
              || character == QLatin1Char('_'))) {
            return {};
        }
    }
    return extension;
}

bool ApplicationLauncher::ensureAssociationSettingsDirectory() const
{
    return QDir().mkpath(QFileInfo(m_associationSettingsPath).absolutePath());
}
