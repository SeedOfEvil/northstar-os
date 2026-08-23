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
    Q_PROPERTY(bool discoverable READ discoverable NOTIFY stateChanged)
    Q_PROPERTY(bool awaitingConfirmation READ awaitingConfirmation NOTIFY stateChanged)
    Q_PROPERTY(QString confirmationCode READ confirmationCode NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY stateChanged)
    Q_PROPERTY(bool fileTransferAvailable READ fileTransferAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool receivingFiles READ receivingFiles NOTIFY stateChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)

public:
    explicit BluetoothController(QObject *parent = nullptr);

    bool busy() const;
    bool discoverable() const;
    bool awaitingConfirmation() const;
    QString confirmationCode() const;
    QString statusMessage() const;
    bool statusIsError() const;
    bool fileTransferAvailable() const;
    bool receivingFiles() const;
    QVariantList devices() const;

    Q_INVOKABLE bool refreshDevices();
    Q_INVOKABLE bool pairDevice(const QString &addressHex, const QString &name);
    Q_INVOKABLE bool respondToPairing(bool accepted);
    Q_INVOKABLE bool forgetDevice(const QString &addressHex);
    Q_INVOKABLE bool setDiscoverable(bool enabled);
    Q_INVOKABLE bool setReceivingFiles(bool enabled);
    Q_INVOKABLE bool sendFile(const QString &addressHex, const QString &fileUrl);

signals:
    void stateChanged();
    void devicesChanged();
    void pairingConfirmationRequested();
    void pairingFinished(bool success);
    void forgetFinished(bool success);
    void authorizationPromptExpected();
    void authorizationCompleted();

private:
    enum class Operation { None, Scan, Pair, Forget, Discoverability, SendFile };

    bool start(Operation operation, const QStringList &arguments);
    bool createRequest(const QByteArray &contents);
    void finish(bool success, const QString &message);
    void parseScan(const QByteArray &output);
    void processOutput(bool flushRemainder = false);

    QProcess *m_process = nullptr;
    QProcess *m_transferServer = nullptr;
    QTemporaryFile *m_request = nullptr;
    QByteArray m_standardOutput;
    QVariantList m_devices;
    Operation m_operation = Operation::None;
    bool m_busy = false;
    bool m_discoverable = false;
    bool m_pendingDiscoverable = false;
    bool m_authorizationPending = false;
    bool m_awaitingConfirmation = false;
    bool m_statusIsError = false;
    bool m_receivingFiles = false;
    QString m_statusMessage;
    QString m_confirmationCode;
    QString m_pairingName;
};
