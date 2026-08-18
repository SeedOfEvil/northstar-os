#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QLocalServer;
class QLocalSocket;

// A user-private control socket the compositor can use to reach the shell.
//
// Shell shortcuts registered as Qt application shortcuts only fire while a
// shell window holds keyboard focus, and the layer-shell panel does not hold
// it. A global desktop shortcut therefore has to be bound in the compositor
// and delivered here, which works no matter which client is focused.
//
// The server accepts a single short command per connection and only ever
// recognises a fixed set of names. It never executes text it is given.
class ShellCommandServer final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
    Q_PROPERTY(QString socketPath READ socketPath CONSTANT)

public:
    explicit ShellCommandServer(QObject *parent = nullptr, QString socketPath = {});
    ~ShellCommandServer() override;

    // Every command the shell will act on. Anything else is refused.
    static QStringList supportedCommands();
    static bool isSupportedCommand(const QString &command);
    static QString defaultSocketPath();
    static qint64 maximumRequestBytes();

    bool listening() const;
    QString socketPath() const;
    QString lastError() const;

    bool start();
    void stop();

signals:
    void commandReceived(const QString &command);
    void listeningChanged();

private:
    void acceptConnection();
    void readFrom(QLocalSocket *connection);
    void finish(QLocalSocket *connection, const QByteArray &reply);

    QLocalServer *m_server = nullptr;
    QString m_socketPath;
    QString m_lastError;
};
