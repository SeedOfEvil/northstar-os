#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

struct PowerCommandResult
{
    bool started = false;
    int exitCode = -1;
    QString standardOutput;
};

class PowerController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString lastAction READ lastAction NOTIFY statusChanged)
    Q_PROPERTY(bool batteryAvailable READ batteryAvailable NOTIFY batteryChanged)
    Q_PROPERTY(int batteryPercentage READ batteryPercentage NOTIFY batteryChanged)
    Q_PROPERTY(bool onAcPower READ onAcPower NOTIFY batteryChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY batteryChanged)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY batteryChanged)
    Q_PROPERTY(bool suspendAvailable READ suspendAvailable NOTIFY powerCapabilitiesChanged)
    Q_PROPERTY(bool lidSwitchAvailable READ lidSwitchAvailable NOTIFY powerCapabilitiesChanged)
    Q_PROPERTY(bool lidSuspendEnabled READ lidSuspendEnabled NOTIFY powerCapabilitiesChanged)

public:
    using PowerFunction = std::function<bool(const QString &action, QString *error)>;
    using CommandFunction = std::function<PowerCommandResult(
        const QString &program, const QStringList &arguments)>;

    explicit PowerController(QObject *parent = nullptr,
                              PowerFunction powerFunction = {},
                              QString helperPath = {},
                              CommandFunction commandFunction = {});

    bool available() const;
    bool busy() const;
    QString statusMessage() const;
    QString lastAction() const;
    bool batteryAvailable() const;
    int batteryPercentage() const;
    bool onAcPower() const;
    bool batteryCharging() const;
    QString batteryStatus() const;
    bool suspendAvailable() const;
    bool lidSwitchAvailable() const;
    bool lidSuspendEnabled() const;

    Q_INVOKABLE bool requestSuspend();
    Q_INVOKABLE bool requestRestart();
    Q_INVOKABLE bool requestShutdown();
    Q_INVOKABLE bool setLidSuspendEnabled(bool enabled);
    Q_INVOKABLE void refreshBattery();
    Q_INVOKABLE void refreshPowerCapabilities();

signals:
    void statusChanged();
    void batteryChanged();
    void powerCapabilitiesChanged();

private:
    bool request(const QString &action);
    bool runHelper(const QString &action, QString *error) const;

    QString m_helperPath;
    PowerFunction m_powerFunction;
    CommandFunction m_commandFunction;
    QTimer m_batteryTimer;
    bool m_available = false;
    bool m_busy = false;
    QString m_statusMessage;
    QString m_lastAction;
    bool m_batteryAvailable = false;
    int m_batteryPercentage = 0;
    bool m_onAcPower = false;
    bool m_batteryCharging = false;
    int m_batteryMinutes = -1;
    QString m_batteryStatus = QStringLiteral("No battery detected");
    bool m_suspendAvailable = false;
    bool m_lidSwitchAvailable = false;
    bool m_lidSuspendEnabled = false;
};
