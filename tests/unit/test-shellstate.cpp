#include "applicationlauncher.h"
#include "shellstate.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class ShellStateTest final : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreStable();
    void activeWindowTitleFallsBackToDesktop();
    void themeToggleEmitsOncePerChange();
    void persistsDarkMode();
    void persistsFilesViewMode();
    void launcherUsesPinnedPrograms();
};

void ShellStateTest::defaultsAreStable()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    ShellState state(nullptr, temporaryDirectory.filePath(QStringLiteral("preferences.ini")));

    QCOMPARE(state.pinnedApplications(), QStringList({QStringLiteral("qterminal"), QStringLiteral("firefox")}));
    QCOMPARE(state.activeWindowTitle(), QStringLiteral("Desktop"));
    QVERIFY(state.darkMode());
    QVERIFY(state.filesGridView());
}

void ShellStateTest::activeWindowTitleFallsBackToDesktop()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    ShellState state(nullptr, temporaryDirectory.filePath(QStringLiteral("preferences.ini")));
    QSignalSpy spy(&state, &ShellState::activeWindowTitleChanged);

    state.setActiveWindowTitle(QStringLiteral("   "));

    QCOMPARE(state.activeWindowTitle(), QStringLiteral("Desktop"));
    QCOMPARE(spy.count(), 0);

    state.setActiveWindowTitle(QStringLiteral("Terminal"));
    QCOMPARE(state.activeWindowTitle(), QStringLiteral("Terminal"));
    QCOMPARE(spy.count(), 1);
}

void ShellStateTest::themeToggleEmitsOncePerChange()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    ShellState state(nullptr, temporaryDirectory.filePath(QStringLiteral("preferences.ini")));
    QSignalSpy spy(&state, &ShellState::darkModeChanged);

    state.toggleDarkMode();
    QVERIFY(!state.darkMode());
    QCOMPARE(spy.count(), 1);

    state.setDarkMode(false);
    QCOMPARE(spy.count(), 1);

    state.setDarkMode(true);
    QVERIFY(state.darkMode());
    QCOMPARE(spy.count(), 2);
}

void ShellStateTest::persistsDarkMode()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = temporaryDirectory.filePath(QStringLiteral("preferences.ini"));

    {
        ShellState state(nullptr, settingsPath);
        QVERIFY(state.darkMode());
        state.setDarkMode(false);
    }

    ShellState restoredState(nullptr, settingsPath);
    QVERIFY(!restoredState.darkMode());
}

void ShellStateTest::persistsFilesViewMode()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = temporaryDirectory.filePath(QStringLiteral("preferences.ini"));

    {
        ShellState state(nullptr, settingsPath);
        QVERIFY(state.filesGridView());
        QSignalSpy spy(&state, &ShellState::filesGridViewChanged);

        state.setFilesGridView(false);

        QVERIFY(!state.filesGridView());
        QCOMPARE(spy.count(), 1);
        state.setFilesGridView(false);
        QCOMPARE(spy.count(), 1);
    }

    ShellState restoredState(nullptr, settingsPath);
    QVERIFY(!restoredState.filesGridView());
}

void ShellStateTest::launcherUsesPinnedPrograms()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QStringList launchedPrograms;
    ApplicationLauncher launcher(
        nullptr,
        [&launchedPrograms](const QString &program, const QStringList &, qint64 *pid) {
            launchedPrograms.append(program);
            if (pid != nullptr) {
                *pid = program == QStringLiteral("qterminal") ? 7001 : 7002;
            }
            return true;
        },
        {},
        temporaryDirectory.filePath(QStringLiteral("launch.log")));

    QVERIFY(launcher.launchTerminal());
    QVERIFY(launcher.launchBrowser());
    QCOMPARE(launchedPrograms, QStringList({QStringLiteral("qterminal"), QStringLiteral("firefox")}));
    QVERIFY(launcher.lastLaunchSucceeded());
    QCOMPARE(launcher.lastLaunchDesktopId(), QStringLiteral("firefox"));
    QCOMPARE(launcher.lastLaunchPid(), 7002);
    QVERIFY(launcher.launchMessage().contains(QStringLiteral("Firefox")));

    QFile launchLog(launcher.launchLogPath());
    QVERIFY(launchLog.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString logContents = QString::fromUtf8(launchLog.readAll());
    QVERIFY(logContents.contains(QStringLiteral("desktop_id=qterminal")));
    QVERIFY(logContents.contains(QStringLiteral("pid=7001")));
    QVERIFY(logContents.contains(QStringLiteral("desktop_id=firefox")));
    QVERIFY(logContents.contains(QStringLiteral("pid=7002")));

    launcher.clearLaunchMessage();
    QVERIFY(launcher.launchMessage().isEmpty());
}

QTEST_MAIN(ShellStateTest)
#include "test-shellstate.moc"
