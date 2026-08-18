#include "shellcommandserver.h"

#include <QDir>
#include <QFile>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class ShellCommandServerTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void listensOnAUserPrivateSocket();
    void deliversASupportedCommand();
    void refusesAnUnknownCommand();
    void refusesAnOverlongRequest();
    void ignoresATruncatedRequest();
    void reclaimsAStaleSocket();
    void stopsListeningAndRemovesItsSocket();
    void namesTheSocketAfterTheWaylandDisplay();

private:
    QString socketPath() const;
    QString sendRequest(const QByteArray &request, bool *connected = nullptr);

    QTemporaryDir *m_directory = nullptr;
};

void ShellCommandServerTest::init()
{
    m_directory = new QTemporaryDir;
    QVERIFY(m_directory->isValid());
}

void ShellCommandServerTest::cleanup()
{
    delete m_directory;
    m_directory = nullptr;
}

QString ShellCommandServerTest::socketPath() const
{
    return m_directory->filePath(QStringLiteral("shell.sock"));
}

QString ShellCommandServerTest::sendRequest(const QByteArray &request, bool *connected)
{
    QLocalSocket socket;
    socket.connectToServer(socketPath());
    const bool established = socket.waitForConnected(2000);
    if (connected != nullptr) {
        *connected = established;
    }
    if (!established) {
        return {};
    }

    socket.write(request);
    socket.waitForBytesWritten(2000);
    if (!socket.waitForReadyRead(2000)) {
        return {};
    }
    return QString::fromUtf8(socket.readAll()).trimmed();
}

void ShellCommandServerTest::listensOnAUserPrivateSocket()
{
    ShellCommandServer server(nullptr, socketPath());
    QVERIFY(!server.listening());
    QVERIFY(server.start());
    QVERIFY(server.listening());
    QCOMPARE(server.socketPath(), socketPath());
    QVERIFY(QFile::exists(socketPath()));

    // The socket carries desktop control, so no other account may reach it.
    const QFile::Permissions permissions = QFile::permissions(socketPath());
    QVERIFY(!permissions.testFlag(QFile::ReadGroup));
    QVERIFY(!permissions.testFlag(QFile::WriteGroup));
    QVERIFY(!permissions.testFlag(QFile::ReadOther));
    QVERIFY(!permissions.testFlag(QFile::WriteOther));

    // Starting twice is harmless.
    QVERIFY(server.start());
}

void ShellCommandServerTest::deliversASupportedCommand()
{
    ShellCommandServer server(nullptr, socketPath());
    QVERIFY(server.start());
    QSignalSpy commands(&server, &ShellCommandServer::commandReceived);

    QCOMPARE(sendRequest(QByteArray("toggle-search\n")), QStringLiteral("ok"));
    QVERIFY(commands.wait(2000) || commands.count() > 0);
    QCOMPARE(commands.count(), 1);
    QCOMPARE(commands.first().at(0).toString(), QStringLiteral("toggle-search"));

    // A second connection is served just as well as the first, which is the
    // whole point: the shortcut must keep working indefinitely.
    QCOMPARE(sendRequest(QByteArray("open-search\n")), QStringLiteral("ok"));
    QTRY_COMPARE(commands.count(), 2);
    QCOMPARE(commands.last().at(0).toString(), QStringLiteral("open-search"));

    QCOMPARE(sendRequest(QByteArray("  toggle-search  \n")), QStringLiteral("ok"));
    QTRY_COMPARE(commands.count(), 3);
}

void ShellCommandServerTest::refusesAnUnknownCommand()
{
    ShellCommandServer server(nullptr, socketPath());
    QVERIFY(server.start());
    QSignalSpy commands(&server, &ShellCommandServer::commandReceived);

    QCOMPARE(sendRequest(QByteArray("rm -rf /\n")), QStringLiteral("error unknown-command"));
    QCOMPARE(sendRequest(QByteArray("open-settings\n")), QStringLiteral("error unknown-command"));
    QCOMPARE(sendRequest(QByteArray("\n")), QStringLiteral("error unknown-command"));

    // Nothing unrecognised is ever handed on to the shell.
    QCOMPARE(commands.count(), 0);

    QVERIFY(ShellCommandServer::isSupportedCommand(QStringLiteral("open-search")));
    QVERIFY(!ShellCommandServer::isSupportedCommand(QStringLiteral("open-anything")));
    QVERIFY(!ShellCommandServer::supportedCommands().isEmpty());
}

void ShellCommandServerTest::refusesAnOverlongRequest()
{
    ShellCommandServer server(nullptr, socketPath());
    QVERIFY(server.start());
    QSignalSpy commands(&server, &ShellCommandServer::commandReceived);

    QByteArray flood(static_cast<int>(ShellCommandServer::maximumRequestBytes()) + 64, 'x');
    flood.append('\n');
    QCOMPARE(sendRequest(flood), QStringLiteral("error too-long"));
    QCOMPARE(commands.count(), 0);
}

void ShellCommandServerTest::ignoresATruncatedRequest()
{
    ShellCommandServer server(nullptr, socketPath());
    QVERIFY(server.start());
    QSignalSpy commands(&server, &ShellCommandServer::commandReceived);

    // A client that never sends a newline must not trigger anything.
    QLocalSocket socket;
    socket.connectToServer(socketPath());
    QVERIFY(socket.waitForConnected(2000));
    socket.write(QByteArray("toggle-sea"));
    socket.waitForBytesWritten(2000);
    QVERIFY(!socket.waitForReadyRead(300));
    QCOMPARE(commands.count(), 0);

    // Completing the line delivers it exactly once.
    socket.write(QByteArray("rch\n"));
    socket.waitForBytesWritten(2000);
    QVERIFY(socket.waitForReadyRead(2000));
    QCOMPARE(QString::fromUtf8(socket.readAll()).trimmed(), QStringLiteral("ok"));
    QTRY_COMPARE(commands.count(), 1);
}

void ShellCommandServerTest::reclaimsAStaleSocket()
{
    // A shell that crashed leaves its socket file behind.
    QFile stale(socketPath());
    QVERIFY(stale.open(QIODevice::WriteOnly));
    stale.write("stale");
    stale.close();
    QVERIFY(QFile::exists(socketPath()));

    ShellCommandServer server(nullptr, socketPath());
    QVERIFY(server.start());
    QVERIFY(server.listening());
    QCOMPARE(sendRequest(QByteArray("open-search\n")), QStringLiteral("ok"));
}

void ShellCommandServerTest::stopsListeningAndRemovesItsSocket()
{
    ShellCommandServer server(nullptr, socketPath());
    QVERIFY(server.start());
    QVERIFY(QFile::exists(socketPath()));

    server.stop();
    QVERIFY(!server.listening());
    QVERIFY(!QFile::exists(socketPath()));

    bool connected = true;
    sendRequest(QByteArray("open-search\n"), &connected);
    QVERIFY(!connected);
}

void ShellCommandServerTest::namesTheSocketAfterTheWaylandDisplay()
{
    const QByteArray previousRuntime = qgetenv("XDG_RUNTIME_DIR");
    const QByteArray previousDisplay = qgetenv("WAYLAND_DISPLAY");

    qputenv("XDG_RUNTIME_DIR", m_directory->path().toUtf8());
    qputenv("WAYLAND_DISPLAY", QByteArray("wayland-7"));
    QCOMPARE(ShellCommandServer::defaultSocketPath(),
             QDir(m_directory->path()).filePath(QStringLiteral("northstar-shell-wayland-7.sock")));

    // Two sessions on one account must not share a socket path.
    qputenv("WAYLAND_DISPLAY", QByteArray("wayland-8"));
    QVERIFY(ShellCommandServer::defaultSocketPath()
            != QDir(m_directory->path()).filePath(QStringLiteral("northstar-shell-wayland-7.sock")));

    qunsetenv("WAYLAND_DISPLAY");
    QCOMPARE(ShellCommandServer::defaultSocketPath(),
             QDir(m_directory->path()).filePath(QStringLiteral("northstar-shell.sock")));

    if (previousRuntime.isEmpty()) {
        qunsetenv("XDG_RUNTIME_DIR");
    } else {
        qputenv("XDG_RUNTIME_DIR", previousRuntime);
    }
    if (previousDisplay.isEmpty()) {
        qunsetenv("WAYLAND_DISPLAY");
    } else {
        qputenv("WAYLAND_DISPLAY", previousDisplay);
    }
}

QTEST_MAIN(ShellCommandServerTest)
#include "test-shellcommandserver.moc"
