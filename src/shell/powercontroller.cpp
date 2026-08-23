#include "powercontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <utility>

namespace {

QString defaultPowerHelperPath()
{
    const QString configuredPath = qEnvironmentVariable("NORTHSTAR_POWER_HELPER");
    if (!configuredPath.isEmpty()) {
        return configuredPath;
    }

    const QString userPath = QDir::home().filePath(QStringLiteral(".local/bin/northstar-power"));
    if (QFileInfo(userPath).isExecutable()) {
        return userPath;
    }

    return QStandardPaths::findExecutable(QStringLiteral("northstar-power"));
}

QString actionLabel(const QString &action)
{
    if (action == QStringLiteral("restart")) {
        return QStringLiteral("Restart");
    }
    if (action == QStringLiteral("shutdown")) {
        return QStringLiteral("Shut down");
    }
    if (action == QStringLiteral("suspend")) {
        return QStringLiteral("Sleep");
    }
    return QStringLiteral("Lid-close sleep");
}

PowerCommandResult runPowerCommand(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(800)) {
        return {};
    }
    if (!process.waitForFinished(800)) {
        process.kill();
        process.waitForFinished();
        return {true, -1, {}};
    }
    return {true, process.exitCode(), QString::fromUtf8(process.readAllStandardOutput())};
}

QString remainingTimeLabel(int minutes)
{
    if (minutes <= 0) {
        return {};
    }
    const int hours = minutes / 60;
    const int remainder = minutes % 60;
    return hours > 0
        ? QStringLiteral("%1h %2m remaining").arg(hours).arg(remainder)
        : QStringLiteral("%1m remaining").arg(remainder);
}

} // namespace

PowerController::PowerController(QObject *parent,
                                 PowerFunction powerFunction,
                                 QString helperPath,
                                 CommandFunction commandFunction)
    : QObject(parent)
    , m_helperPath(helperPath.isEmpty() ? defaultPowerHelperPath() : std::move(helperPath))
    , m_powerFunction(std::move(powerFunction))
    , m_commandFunction(commandFunction ? std::move(commandFunction) : runPowerCommand)
{
    if (!m_powerFunction) {
        m_powerFunction = [this](const QString &action, QString *error) {
            return runHelper(action, error);
        };
        m_available = !m_helperPath.isEmpty() && QFileInfo(m_helperPath).isExecutable();
    } else {
        m_available = true;
    }

    connect(&m_batteryTimer, &QTimer::timeout, this, &PowerController::refreshBattery);
    m_batteryTimer.start(30000);
    refreshBattery();
    refreshPowerCapabilities();
}

bool PowerController::available() const
{
    return m_available;
}

bool PowerController::busy() const
{
    return m_busy;
}

QString PowerController::statusMessage() const
{
    return m_statusMessage;
}

QString PowerController::lastAction() const
{
    return m_lastAction;
}

bool PowerController::batteryAvailable() const { return m_batteryAvailable; }
int PowerController::batteryPercentage() const { return m_batteryPercentage; }
bool PowerController::onAcPower() const { return m_onAcPower; }
bool PowerController::batteryCharging() const { return m_batteryCharging; }
QString PowerController::batteryStatus() const { return m_batteryStatus; }
bool PowerController::suspendAvailable() const { return m_suspendAvailable; }
bool PowerController::lidSwitchAvailable() const { return m_lidSwitchAvailable; }
bool PowerController::lidSuspendEnabled() const { return m_lidSuspendEnabled; }

void PowerController::refreshBattery()
{
    const PowerCommandResult result = m_commandFunction(
        QStringLiteral("/sbin/sysctl"),
        {QStringLiteral("-n"), QStringLiteral("hw.acpi.battery.units"),
         QStringLiteral("hw.acpi.battery.life"), QStringLiteral("hw.acpi.battery.state"),
         QStringLiteral("hw.acpi.battery.time"), QStringLiteral("hw.acpi.acline")});

    const QStringList lines = result.standardOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    bool unitsOk = false;
    bool lifeOk = false;
    bool stateOk = false;
    bool timeOk = false;
    bool acOk = false;
    const int units = lines.value(0).trimmed().toInt(&unitsOk);
    const int life = lines.value(1).trimmed().toInt(&lifeOk);
    const int state = lines.value(2).trimmed().toInt(&stateOk);
    const int minutes = lines.value(3).trimmed().toInt(&timeOk);
    const int acLine = lines.value(4).trimmed().toInt(&acOk);
    const bool available = result.started && result.exitCode == 0 && lines.size() >= 5
        && unitsOk && lifeOk && stateOk && timeOk && acOk && units > 0;

    bool onAc = false;
    bool charging = false;
    int percentage = 0;
    QString status = QStringLiteral("No battery detected");
    if (available) {
        percentage = qBound(0, life, 100);
        onAc = acLine != 0;
        charging = (state & 0x02) != 0;
        if (charging) {
            status = QStringLiteral("Charging - %1%").arg(percentage);
        } else if (onAc && percentage >= 100) {
            status = QStringLiteral("Fully charged");
        } else if (onAc) {
            status = QStringLiteral("Plugged in - %1%").arg(percentage);
        } else {
            const QString remaining = remainingTimeLabel(minutes);
            status = remaining.isEmpty()
                ? QStringLiteral("On battery - %1%").arg(percentage)
                : QStringLiteral("%1% - %2").arg(percentage).arg(remaining);
        }
    }

    const bool changed = m_batteryAvailable != available
        || m_batteryPercentage != percentage || m_onAcPower != onAc
        || m_batteryCharging != charging || m_batteryMinutes != minutes
        || m_batteryStatus != status;
    m_batteryAvailable = available;
    m_batteryPercentage = percentage;
    m_onAcPower = onAc;
    m_batteryCharging = charging;
    m_batteryMinutes = available ? minutes : -1;
    m_batteryStatus = status;
    if (changed) {
        emit batteryChanged();
    }
}

void PowerController::refreshPowerCapabilities()
{
    const PowerCommandResult result = m_commandFunction(
        QStringLiteral("/sbin/sysctl"),
        {QStringLiteral("-n"), QStringLiteral("hw.acpi.suspend_state"),
         QStringLiteral("hw.acpi.lid_switch_state")});
    const QStringList lines = result.standardOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const QString suspendState = lines.value(0).trimmed().toUpper();
    const QString lidState = lines.value(1).trimmed().toUpper();
    const bool readable = result.started && result.exitCode == 0 && lines.size() >= 2;
    const bool suspendAvailable = m_available && readable
        && suspendState == QStringLiteral("S3");
    const bool lidAvailable = m_available && readable && !lidState.isEmpty();
    const bool lidEnabled = lidAvailable && lidState == QStringLiteral("S3");

    if (m_suspendAvailable == suspendAvailable && m_lidSwitchAvailable == lidAvailable
        && m_lidSuspendEnabled == lidEnabled) {
        return;
    }
    m_suspendAvailable = suspendAvailable;
    m_lidSwitchAvailable = lidAvailable;
    m_lidSuspendEnabled = lidEnabled;
    emit powerCapabilitiesChanged();
}

bool PowerController::requestSuspend()
{
    return request(QStringLiteral("suspend"));
}

bool PowerController::requestRestart()
{
    return request(QStringLiteral("restart"));
}

bool PowerController::requestShutdown()
{
    return request(QStringLiteral("shutdown"));
}

bool PowerController::setLidSuspendEnabled(bool enabled)
{
    if (!request(enabled ? QStringLiteral("lid-suspend-on")
                         : QStringLiteral("lid-suspend-off"))) {
        return false;
    }
    refreshPowerCapabilities();
    if (!m_lidSwitchAvailable || m_lidSuspendEnabled != enabled) {
        m_statusMessage = QStringLiteral("Lid-close sleep changed, but FreeBSD did not confirm it.");
        emit statusChanged();
        return false;
    }
    m_statusMessage = enabled ? QStringLiteral("Closing the lid will put Northstar to sleep.")
                              : QStringLiteral("Closing the lid will not put Northstar to sleep.");
    emit statusChanged();
    return true;
}

bool PowerController::request(const QString &action)
{
    if (m_busy) {
        return false;
    }

    m_lastAction = action;
    m_busy = true;
    emit statusChanged();

    const bool supported = m_available
        && (action != QStringLiteral("suspend") || m_suspendAvailable)
        && (!action.startsWith(QStringLiteral("lid-suspend-")) || m_lidSwitchAvailable);
    QString error;
    const bool succeeded = supported && m_powerFunction(action, &error);
    if (succeeded) {
        m_statusMessage = QStringLiteral("%1 requested").arg(actionLabel(action));
    } else if (!supported) {
        m_statusMessage = action == QStringLiteral("suspend")
            ? QStringLiteral("Sleep is not supported by this system.")
            : action.startsWith(QStringLiteral("lid-suspend-"))
            ? QStringLiteral("A configurable lid switch was not detected.")
            : QStringLiteral("Power controls are not configured.");
    } else if (error.isEmpty()) {
        m_statusMessage = QStringLiteral("%1 request failed.").arg(actionLabel(action));
    } else {
        m_statusMessage = QStringLiteral("%1 request failed: %2").arg(actionLabel(action), error);
    }

    m_busy = false;
    emit statusChanged();
    return succeeded;
}

bool PowerController::runHelper(const QString &action, QString *error) const
{
    if (m_helperPath.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("helper is unavailable");
        }
        return false;
    }

    QProcess process;
    process.start(m_helperPath, {action});
    if (!process.waitForStarted(3000)) {
        if (error != nullptr) {
            *error = QStringLiteral("helper could not start");
        }
        return false;
    }

    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        if (error != nullptr) {
            *error = QStringLiteral("helper timed out");
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error != nullptr) {
            *error = QStringLiteral("helper exited with status %1").arg(process.exitCode());
        }
        return false;
    }

    return true;
}
