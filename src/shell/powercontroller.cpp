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
    return action == QStringLiteral("restart") ? QStringLiteral("Restart") : QStringLiteral("Shut down");
}

} // namespace

PowerController::PowerController(QObject *parent,
                                 PowerFunction powerFunction,
                                 QString helperPath)
    : QObject(parent)
    , m_helperPath(helperPath.isEmpty() ? defaultPowerHelperPath() : std::move(helperPath))
    , m_powerFunction(std::move(powerFunction))
{
    if (!m_powerFunction) {
        m_powerFunction = [this](const QString &action, QString *error) {
            return runHelper(action, error);
        };
        m_available = !m_helperPath.isEmpty() && QFileInfo(m_helperPath).isExecutable();
    } else {
        m_available = true;
    }
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

bool PowerController::requestRestart()
{
    return request(QStringLiteral("restart"));
}

bool PowerController::requestShutdown()
{
    return request(QStringLiteral("shutdown"));
}

bool PowerController::request(const QString &action)
{
    if (m_busy) {
        return false;
    }

    m_lastAction = action;
    m_busy = true;
    emit statusChanged();

    QString error;
    const bool succeeded = m_available && m_powerFunction(action, &error);
    if (succeeded) {
        m_statusMessage = QStringLiteral("%1 requested").arg(actionLabel(action));
    } else if (!m_available) {
        m_statusMessage = QStringLiteral("Power controls are not configured.");
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
