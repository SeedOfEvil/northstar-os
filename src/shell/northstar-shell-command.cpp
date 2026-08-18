#include "shellcommandserver.h"

#include <QCoreApplication>
#include <QLocalSocket>
#include <QTextStream>

// Sends one control command to the running Northstar shell.
//
// The compositor binds a global key to this program, which is how a shortcut
// reaches the shell no matter which client currently holds keyboard focus.
int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-shell-command"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QTextStream error(stderr);
    const QStringList arguments = application.arguments();
    if (arguments.size() != 2) {
        error << QStringLiteral("Usage: northstar-shell-command COMMAND\n")
              << QStringLiteral("Commands: ")
              << ShellCommandServer::supportedCommands().join(QStringLiteral(", "))
              << QStringLiteral("\n");
        return 2;
    }

    const QString command = arguments.at(1).trimmed();
    if (!ShellCommandServer::isSupportedCommand(command)) {
        error << QStringLiteral("Unknown command: %1\n").arg(command)
              << QStringLiteral("Commands: ")
              << ShellCommandServer::supportedCommands().join(QStringLiteral(", "))
              << QStringLiteral("\n");
        return 2;
    }

    const QString socketPath = ShellCommandServer::resolveSocketPath();
    if (socketPath.isEmpty()) {
        error << QStringLiteral("More than one Northstar shell socket is present in %1; "
                                "set WAYLAND_DISPLAY to choose one.
")
                     .arg(ShellCommandServer::runtimeDirectory());
        return 1;
    }
    QLocalSocket socket;
    socket.connectToServer(socketPath);
    if (!socket.waitForConnected(2000)) {
        error << QStringLiteral("No Northstar shell is listening on %1\n").arg(socketPath);
        return 1;
    }

    const QByteArray request = command.toUtf8() + '\n';
    if (socket.write(request) != request.size() || !socket.waitForBytesWritten(2000)) {
        error << QStringLiteral("Unable to send %1 to the shell\n").arg(command);
        return 1;
    }

    if (!socket.waitForReadyRead(2000)) {
        error << QStringLiteral("The shell did not answer %1\n").arg(command);
        return 1;
    }

    const QString reply = QString::fromUtf8(socket.readAll()).trimmed();
    if (reply != QStringLiteral("ok")) {
        error << QStringLiteral("The shell refused %1: %2\n").arg(command, reply);
        return 1;
    }
    return 0;
}
