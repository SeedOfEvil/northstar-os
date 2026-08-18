#include "shellcommandserver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

namespace {

constexpr qint64 MaximumRequestBytes = 256;

} // namespace

ShellCommandServer::ShellCommandServer(QObject *parent, QString socketPath)
    : QObject(parent)
    , m_socketPath(socketPath.trimmed().isEmpty() ? defaultSocketPath()
                                                  : std::move(socketPath))
{
}

ShellCommandServer::~ShellCommandServer()
{
    stop();
}

QStringList ShellCommandServer::supportedCommands()
{
    return QStringList{
        QStringLiteral("open-search"),
        QStringLiteral("toggle-search"),
    };
}

bool ShellCommandServer::isSupportedCommand(const QString &command)
{
    return supportedCommands().contains(command);
}

qint64 ShellCommandServer::maximumRequestBytes()
{
    return MaximumRequestBytes;
}

QString ShellCommandServer::defaultSocketPath()
{
    QString runtimeDirectory = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtimeDirectory.isEmpty()) {
        runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    }
    if (runtimeDirectory.isEmpty()) {
        runtimeDirectory = QDir::tempPath();
    }

    // One socket per Wayland display keeps two sessions on one account from
    // fighting over the same path.
    const QString display = qEnvironmentVariable("WAYLAND_DISPLAY");
    const QString name = display.isEmpty()
        ? QStringLiteral("northstar-shell.sock")
        : QStringLiteral("northstar-shell-%1.sock").arg(display);
    return QDir(runtimeDirectory).filePath(name);
}

bool ShellCommandServer::listening() const
{
    return m_server != nullptr && m_server->isListening();
}

QString ShellCommandServer::socketPath() const
{
    return m_socketPath;
}

QString ShellCommandServer::lastError() const
{
    return m_lastError;
}

bool ShellCommandServer::start()
{
    if (listening()) {
        return true;
    }

    if (!QDir().mkpath(QFileInfo(m_socketPath).absolutePath())) {
        m_lastError = QStringLiteral("Unable to create the runtime directory for the shell socket.");
        return false;
    }

    // A shell that crashed leaves its socket behind; the path is inside the
    // user's own runtime directory, so reclaiming it is safe.
    QLocalServer::removeServer(m_socketPath);

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, &ShellCommandServer::acceptConnection);

    if (!m_server->listen(m_socketPath)) {
        m_lastError = m_server->errorString();
        m_server->deleteLater();
        m_server = nullptr;
        return false;
    }

    m_lastError.clear();
    emit listeningChanged();
    return true;
}

void ShellCommandServer::stop()
{
    if (m_server == nullptr) {
        return;
    }
    m_server->close();
    m_server->deleteLater();
    m_server = nullptr;
    QLocalServer::removeServer(m_socketPath);
    emit listeningChanged();
}

void ShellCommandServer::acceptConnection()
{
    while (m_server != nullptr && m_server->hasPendingConnections()) {
        QLocalSocket *connection = m_server->nextPendingConnection();
        if (connection == nullptr) {
            continue;
        }
        connect(connection, &QLocalSocket::readyRead, this,
                [this, connection]() { readFrom(connection); });
        connect(connection, &QLocalSocket::disconnected, connection, &QObject::deleteLater);
        readFrom(connection);
    }
}

void ShellCommandServer::readFrom(QLocalSocket *connection)
{
    if (connection == nullptr) {
        return;
    }

    // A control command is a short single line. Anything longer is refused
    // outright rather than buffered.
    if (connection->bytesAvailable() > MaximumRequestBytes) {
        finish(connection, QByteArray("error too-long\n"));
        return;
    }

    if (!connection->canReadLine()) {
        return;
    }

    const QString request = QString::fromUtf8(connection->readLine()).trimmed();
    if (!isSupportedCommand(request)) {
        finish(connection, QByteArray("error unknown-command\n"));
        return;
    }

    finish(connection, QByteArray("ok\n"));
    emit commandReceived(request);
}

void ShellCommandServer::finish(QLocalSocket *connection, const QByteArray &reply)
{
    connection->write(reply);
    connection->flush();
    connection->disconnectFromServer();
}
