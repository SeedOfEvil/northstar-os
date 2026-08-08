#include "sessioncontroller.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <csignal>
#include <unistd.h>

class SessionControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsStatusContract();
    void missingStatusIsUnavailable();
    void rejectsEndRequestFromUnexpectedParent();
    void requestsEndSessionThroughExactParent();
};

void SessionControllerTest::readsStatusContract()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString statusPath = temporaryDirectory.filePath(QStringLiteral("session.status"));
    QFile statusFile(statusPath);
    QVERIFY(statusFile.open(QIODevice::WriteOnly | QIODevice::Text));
    statusFile.write("status_version=1\n"
                     "state=running\n"
                     "supervisor_pid=1234\n"
                     "compositor_pid=1235\n"
                     "shell_pid=1236\n"
                     "wayland_display=wayland-1\n"
                     "restart_count=2\n"
                     "last_event=shell-started\n");
    statusFile.close();

    SessionController controller(statusPath, temporaryDirectory.filePath(QStringLiteral("session.control")), 1234);

    QVERIFY(controller.available());
    QCOMPARE(controller.state(), QStringLiteral("running"));
    QCOMPARE(controller.waylandDisplay(), QStringLiteral("wayland-1"));
    QCOMPARE(controller.supervisorPid(), 1234);
    QCOMPARE(controller.compositorPid(), 1235);
    QCOMPARE(controller.shellPid(), 1236);
    QCOMPARE(controller.restartCount(), 2);
    QCOMPARE(controller.lastEvent(), QStringLiteral("shell-started"));
}

void SessionControllerTest::missingStatusIsUnavailable()
{
    SessionController controller(QStringLiteral("/tmp/northstar-session-status-does-not-exist"),
                                 QStringLiteral("/tmp/northstar-session-control-does-not-exist"),
                                 1234);

    QVERIFY(!controller.available());
    QCOMPARE(controller.state(), QStringLiteral("Not supervised"));
    QCOMPARE(controller.supervisorPid(), 0);
}

void SessionControllerTest::rejectsEndRequestFromUnexpectedParent()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString statusPath = temporaryDirectory.filePath(QStringLiteral("session.status"));
    QFile statusFile(statusPath);
    QVERIFY(statusFile.open(QIODevice::WriteOnly | QIODevice::Text));
    statusFile.write("state=running\n"
                     "supervisor_pid=1234\n"
                     "wayland_display=wayland-1\n");
    statusFile.close();

    const QString controlPath = temporaryDirectory.filePath(QStringLiteral("session.control"));
    SessionController controller(statusPath, controlPath, 1234);

    QVERIFY(!controller.requestEndSession());
    QVERIFY(!QFile::exists(controlPath));
}

void SessionControllerTest::requestsEndSessionThroughExactParent()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const qint64 expectedSupervisorPid = static_cast<qint64>(::getppid());
    const QString statusPath = temporaryDirectory.filePath(QStringLiteral("session.status"));
    QFile statusFile(statusPath);
    QVERIFY(statusFile.open(QIODevice::WriteOnly | QIODevice::Text));
    statusFile.write("state=running\nsupervisor_pid=");
    statusFile.write(QByteArray::number(expectedSupervisorPid));
    statusFile.write("\nwayland_display=wayland-1\n");
    statusFile.close();

    const QString controlPath = temporaryDirectory.filePath(QStringLiteral("session.control"));
    qint64 signaledPid = 0;
    int signaledSignal = 0;
    SessionController controller(
        statusPath,
        controlPath,
        expectedSupervisorPid,
        nullptr,
        [&signaledPid, &signaledSignal](qint64 pid, int signal) {
            signaledPid = pid;
            signaledSignal = signal;
            return 0;
        });

    QVERIFY(controller.requestEndSession());
    QCOMPARE(signaledPid, expectedSupervisorPid);
    QCOMPARE(signaledSignal, SIGTERM);
    QCOMPARE(controller.state(), QStringLiteral("stopping"));

    QFile controlFile(controlPath);
    QVERIFY(controlFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(controlFile.readAll(), QByteArray("end-session\n"));
}

QTEST_MAIN(SessionControllerTest)
#include "test-sessioncontroller.moc"
