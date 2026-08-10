#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QStringList>

struct QuickSettingsCommandResult
{
    bool started = false;
    int exitCode = -1;
    QString standardOutput;
    QString standardError;
};

class QuickSettingsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool wifiAvailable READ wifiAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY capabilitiesChanged)
    Q_PROPERTY(QString wifiStatus READ wifiStatus NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool bluetoothAvailable READ bluetoothAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool bluetoothEnabled READ bluetoothEnabled NOTIFY capabilitiesChanged)
    Q_PROPERTY(QString bluetoothStatus READ bluetoothStatus NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool soundAvailable READ soundAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(int volume READ volume NOTIFY capabilitiesChanged)
    Q_PROPERTY(QString soundStatus READ soundStatus NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool displayAvailable READ displayAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(int displayBrightness READ displayBrightness NOTIFY capabilitiesChanged)
    Q_PROPERTY(QString displayStatus READ displayStatus NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool nightLightAvailable READ nightLightAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool nightLightEnabled READ nightLightEnabled NOTIFY capabilitiesChanged)
    Q_PROPERTY(QString nightLightStatus READ nightLightStatus NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY doNotDisturbChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    using CommandProvider = std::function<QuickSettingsCommandResult(
        const QString &program, const QStringList &arguments)>;

    explicit QuickSettingsController(QObject *parent = nullptr,
                                     QString settingsPath = {},
                                     CommandProvider commandProvider = {});

    bool wifiAvailable() const;
    bool wifiEnabled() const;
    QString wifiStatus() const;
    bool bluetoothAvailable() const;
    bool bluetoothEnabled() const;
    QString bluetoothStatus() const;
    bool soundAvailable() const;
    int volume() const;
    QString soundStatus() const;
    bool displayAvailable() const;
    int displayBrightness() const;
    QString displayStatus() const;
    bool nightLightAvailable() const;
    bool nightLightEnabled() const;
    QString nightLightStatus() const;
    bool doNotDisturb() const;
    QString statusMessage() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setVolume(int volume);
    Q_INVOKABLE void toggleDoNotDisturb();

public slots:
    void setDoNotDisturb(bool enabled);

signals:
    void capabilitiesChanged();
    void doNotDisturbChanged();
    void statusMessageChanged();

private:
    static QuickSettingsCommandResult runCommand(const QString &program,
                                                  const QStringList &arguments);
    static QString defaultSettingsPath();
    void refreshWifi();
    void refreshBluetooth();
    void refreshSound();
    void refreshDisplay();
    void setStatusMessage(const QString &message);

    QString m_settingsPath;
    CommandProvider m_commandProvider;
    bool m_wifiAvailable = false;
    bool m_wifiEnabled = false;
    QString m_wifiStatus = QStringLiteral("No wireless interface detected");
    bool m_bluetoothAvailable = false;
    bool m_bluetoothEnabled = false;
    QString m_bluetoothStatus = QStringLiteral("No Bluetooth adapter detected");
    bool m_soundAvailable = false;
    int m_volume = 0;
    QString m_soundStatus = QStringLiteral("No mixer device available");
    bool m_displayAvailable = false;
    int m_displayBrightness = 0;
    QString m_displayStatus = QStringLiteral("Brightness control unavailable");
    bool m_nightLightAvailable = false;
    bool m_nightLightEnabled = false;
    QString m_nightLightStatus = QStringLiteral("Compositor color control unavailable");
    bool m_doNotDisturb = false;
    QString m_statusMessage;
};
