#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantList>

class QProcess;
class QTemporaryFile;

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
    Q_INVOKABLE bool pairDevice(const QString &addressHex,
                                const QString &name,
                                const QString &pin);

signals:
    void stateChanged();
    void devicesChanged();
    void secretsCleared();
    void pairingFinished(bool success);
    void authorizationPromptExpected();
    void authorizationCompleted();

private:
    enum class Operation { None, Scan, Pair };

    bool start(Operation operation, const QStringList &arguments);
    void finish(bool success, const QString &message);
    void parseScan(const QByteArray &output);
    static void clearBytes(QByteArray &bytes);

    QProcess *m_process = nullptr;
    QTemporaryFile *m_request = nullptr;
    QByteArray m_pendingSecret;
    QVariantList m_devices;
    Operation m_operation = Operation::None;
    bool m_busy = false;
    bool m_authorizationPending = false;
    bool m_statusIsError = false;
    QString m_statusMessage;
};
