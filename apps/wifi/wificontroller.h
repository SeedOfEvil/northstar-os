#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantList>

class QProcess;
class QTemporaryFile;

class WifiController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY stateChanged)
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)

public:
    explicit WifiController(QObject *parent = nullptr);

    bool busy() const;
    QString statusMessage() const;
    bool statusIsError() const;
    QVariantList networks() const;

    Q_INVOKABLE bool refreshNetworks();
    Q_INVOKABLE bool connectNetwork(const QString &ssidHex,
                                    const QString &security,
                                    const QString &passphrase);

signals:
    void stateChanged();
    void networksChanged();
    void secretsCleared();
    void connectionFinished(bool success);

private:
    enum class Operation { None, Scan, Connect };

    bool start(Operation operation, const QStringList &arguments);
    void finish(bool success, const QString &message);
    void parseScan(const QByteArray &output);
    static void clearBytes(QByteArray &bytes);

    QProcess *m_process = nullptr;
    QTemporaryFile *m_request = nullptr;
    QByteArray m_pendingSecret;
    QVariantList m_networks;
    Operation m_operation = Operation::None;
    bool m_busy = false;
    bool m_statusIsError = false;
    QString m_statusMessage;
};
