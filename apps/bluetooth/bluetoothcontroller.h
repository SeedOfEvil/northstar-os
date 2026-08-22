#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class QProcess;

class BluetoothController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY stateChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)

public:
    explicit BluetoothController(QObject *parent = nullptr);

    bool busy() const;
    QString statusMessage() const;
    bool statusIsError() const;
    QVariantList devices() const;

    Q_INVOKABLE bool refreshDevices();
    Q_INVOKABLE bool openSetupWizard(const QString &addressHex);

signals:
    void stateChanged();
    void devicesChanged();
    void setupWizardLaunched();

private:
    void finish(bool success, const QString &message);
    void parseScan(const QByteArray &output);

    QProcess *m_process = nullptr;
    QVariantList m_devices;
    bool m_busy = false;
    bool m_statusIsError = false;
    QString m_statusMessage;
};
