#include "applicationlauncher.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QProcess>

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

QString logValue(const QString &value)
{
    QString result = value;
    result.replace(QLatin1Char('\n'), QLatin1Char(' '));
    result.replace(QLatin1Char('\r'), QLatin1Char(' '));
    result.replace(QLatin1Char('='), QLatin1Char('_'));
    return result;
}

} // namespace

ApplicationLauncher::ApplicationLauncher(
    QObject *parent,
    LaunchFunction launchFunction,
    QStringList applicationDirectories,
    QString launchLogPath)
    : QObject(parent)
    , m_catalog(std::move(applicationDirectories))
    , m_launchFunction(std::move(launchFunction))
    , m_launchLogPath(launchLogPath.isEmpty() ? defaultLaunchLogPath() : std::move(launchLogPath))
{
    if (!m_launchFunction) {
        m_launchFunction = [](const QString &program, const QStringList &arguments, qint64 *pid) {
            return QProcess::startDetached(program, arguments, QString(), pid);
        };
    }

    connect(&m_catalog, &ApplicationCatalog::applicationsChanged, this, &ApplicationLauncher::applicationsChanged);
    connect(&m_catalog, &ApplicationCatalog::applicationsChanged, this, &ApplicationLauncher::matchingApplicationsChanged);
}

QVariantList ApplicationLauncher::applications() const
{
    return m_catalog.applications();
}

QString ApplicationLauncher::applicationQuery() const
{
    return m_applicationQuery;
}

QVariantList ApplicationLauncher::matchingApplications() const
{
    return m_catalog.searchApplications(m_applicationQuery);
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
    if (!m_catalog.launchSpec(desktopId, &program, &arguments)) {
        setLaunchStatus(desktopId, applicationNameFor(desktopId), {}, 0, false);
        return false;
    }

    return launch(desktopId, applicationNameFor(desktopId), program, arguments);
}

bool ApplicationLauncher::refreshApplications()
{
    return m_catalog.reload();
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
    return desktopId;
}
