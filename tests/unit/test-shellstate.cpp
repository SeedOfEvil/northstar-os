#include "applicationlauncher.h"
#include "shellstate.h"

#include <QtTest/QtTest>

class ShellStateTest final : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreStable();
    void activeWindowTitleFallsBackToDesktop();
    void themeToggleEmitsOncePerChange();
    void launcherUsesPinnedPrograms();
};

void ShellStateTest::defaultsAreStable()
{
    ShellState state;

    QCOMPARE(state.pinnedApplications(), QStringList({QStringLiteral("qterminal"), QStringLiteral("firefox")}));
    QCOMPARE(state.activeWindowTitle(), QStringLiteral("Desktop"));
    QVERIFY(state.darkMode());
}

void ShellStateTest::activeWindowTitleFallsBackToDesktop()
{
    ShellState state;
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
    ShellState state;
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

void ShellStateTest::launcherUsesPinnedPrograms()
{
    QStringList launchedPrograms;
    ApplicationLauncher launcher(nullptr, [&launchedPrograms](const QString &program, const QStringList &) {
        launchedPrograms.append(program);
        return true;
    });

    QVERIFY(launcher.launchTerminal());
    QVERIFY(launcher.launchBrowser());
    QCOMPARE(launchedPrograms, QStringList({QStringLiteral("qterminal"), QStringLiteral("firefox")}));
}

QTEST_MAIN(ShellStateTest)
#include "test-shellstate.moc"
