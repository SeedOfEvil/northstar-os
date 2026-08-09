#include "sessioncontroller.h"

#include <QByteArray>
#include <QFile>
#include <QFileDevice>
#include <QHash>
#include <QIODevice>
#include <QStringList>

#include <csignal>
#include <utility>
#include <unistd.h>

namespace {

qint64 parseInteger(const QString &value)
{
    bool ok = false;
    const qint64 parsed = value.toLongLong(&ok);
    return ok && parsed > 0 ? parsed : 0;
}

int parseCount(const QString &value)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    return ok && parsed >= 0 ? parsed : 0;
}

} // namespace

SessionController::SessionController(QObject *parent)
    : SessionController(qEnvironmentVariable("NORTHSTAR_SESSION_STATUS_FILE"),
                        qEnvironmentVariable("NORTHSTAR_SESSION_CONTROL_FILE"),
                        parseInteger(qEnvironmentVariable("NORTHSTAR_SESSION_SUPERVISOR_PID")),
                        parent)
{
}

SessionController::SessionController(const QString &statusFile,
                                     const QString &controlFile,
                                     qint64 expectedSupervisorPid,
                                     QObject *parent,
                                     SignalFunction signalFunction)
    : QObject(parent)
    , m_statusFile(statusFile)
    , m_controlFile(controlFile)
    , m_expectedSupervisorPid(expectedSupervisorPid)
    , m_signalFunction(std::move(signalFunction))
{
    refresh();
}

bool SessionController::available() const
{
    return m_available;
}

QString SessionController::state() const
{
    return m_state;
}

QString SessionController::waylandDisplay() const
{
    return m_waylandDisplay;
}

qint64 SessionController::supervisorPid() const
{
    return m_supervisorPid;
}

qint64 SessionController::compositorPid() const
{
    return m_compositorPid;
}

qint64 SessionController::shellPid() const
{
    return m_shellPid;
}

int SessionController::restartCount() const
{
    return m_restartCount;
}

QString SessionController::lastEvent() const
{
    return m_lastEvent;
}

bool SessionController::restartable() const
{
    return m_available
        && m_shellPid == static_cast<qint64>(::getpid())
        && m_supervisorPid > 1
        && static_cast<qint64>(::getppid()) == m_supervisorPid;
}

void SessionController::clearStatus()
{
    m_available = false;
    m_state = QStringLiteral("Not supervised");
    m_waylandDisplay.clear();
    m_supervisorPid = 0;
    m_compositorPid = 0;
    m_shellPid = 0;
    m_restartCount = 0;
    m_lastEvent.clear();
}

void SessionController::refresh()
{
    if (m_statusFile.isEmpty()) {
        clearStatus();
        emit statusChanged();
        return;
    }

    QFile statusFile(m_statusFile);
    if (!statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        clearStatus();
        emit statusChanged();
        return;
    }

    QHash<QString, QString> values;
    const QStringList lines = QString::fromUtf8(statusFile.readAll()).split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const int separator = line.indexOf('=');
        if (separator <= 0) {
            continue;
        }
        values.insert(line.left(separator), line.mid(separator + 1));
    }

    const QString nextState = values.value(QStringLiteral("state"));
    const qint64 nextSupervisorPid = parseInteger(values.value(QStringLiteral("supervisor_pid")));
    if (nextState.isEmpty() || nextSupervisorPid <= 1
        || (m_expectedSupervisorPid > 1 && nextSupervisorPid != m_expectedSupervisorPid)) {
        clearStatus();
        emit statusChanged();
        return;
    }

    m_state = nextState;
    m_waylandDisplay = values.value(QStringLiteral("wayland_display"));
    m_supervisorPid = nextSupervisorPid;
    m_compositorPid = parseInteger(values.value(QStringLiteral("compositor_pid")));
    m_shellPid = parseInteger(values.value(QStringLiteral("shell_pid")));
    m_restartCount = parseCount(values.value(QStringLiteral("restart_count")));
    m_lastEvent = values.value(QStringLiteral("last_event"));
    m_available = m_state != QStringLiteral("stopped") && m_state != QStringLiteral("failed");
    emit statusChanged();
}

bool SessionController::requestEndSession()
{
    refresh();
    if (!m_available || m_controlFile.isEmpty() || m_supervisorPid <= 1
        || (m_expectedSupervisorPid > 1 && m_supervisorPid != m_expectedSupervisorPid)
        || static_cast<qint64>(::getppid()) != m_supervisorPid) {
        return false;
    }

    QFile controlFile(m_controlFile);
    if (!controlFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    const QByteArray request("end-session\n");
    if (controlFile.write(request) != request.size()) {
        return false;
    }
    controlFile.close();
    controlFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    const auto signalFunction = m_signalFunction ? m_signalFunction : [](qint64 pid, int signal) {
        return ::kill(static_cast<pid_t>(pid), signal);
    };
    if (signalFunction(m_supervisorPid, SIGTERM) != 0) {
        return false;
    }

    m_state = QStringLiteral("stopping");
    m_lastEvent = QStringLiteral("end-session-requested");
    emit statusChanged();
    return true;
}

bool SessionController::requestShellRestart()
{
    refresh();
    if (!restartable()) {
        return false;
    }

    const auto signalFunction = m_signalFunction ? m_signalFunction : [](qint64 pid, int signal) {
        return ::kill(static_cast<pid_t>(pid), signal);
    };
    if (signalFunction(m_shellPid, SIGTERM) != 0) {
        return false;
    }

    m_state = QStringLiteral("restarting");
    m_lastEvent = QStringLiteral("shell-restart-requested");
    emit statusChanged();
    return true;
}
