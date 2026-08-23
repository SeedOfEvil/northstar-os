#include "powercontroller.h"

#include <QtTest/QtTest>

class PowerControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void requestsRestartThroughControlledAction();
    void reportsPowerActionFailure();
    void requestsSuspendOnlyWhenAcpiSupportsIt();
    void persistsAndConfirmsLidSleep();
    void reportsBatteryAndAcState();
    void reportsUnknownBatteryTimeHonestly();
};

void PowerControllerTest::requestsRestartThroughControlledAction()
{
    QString requestedAction;
    PowerController controller(
        nullptr,
        [&requestedAction](const QString &action, QString *) {
            requestedAction = action;
            return true;
        });

    QVERIFY(controller.available());
    QVERIFY(controller.requestRestart());
    QCOMPARE(requestedAction, QStringLiteral("restart"));
    QCOMPARE(controller.lastAction(), QStringLiteral("restart"));
    QCOMPARE(controller.statusMessage(), QStringLiteral("Restart requested"));
    QVERIFY(!controller.busy());
}

void PowerControllerTest::reportsPowerActionFailure()
{
    PowerController controller(
        nullptr,
        [](const QString &, QString *error) {
            *error = QStringLiteral("not authorized");
            return false;
        });

    QVERIFY(!controller.requestShutdown());
    QCOMPARE(controller.lastAction(), QStringLiteral("shutdown"));
    QCOMPARE(controller.statusMessage(), QStringLiteral("Shut down request failed: not authorized"));
    QVERIFY(!controller.busy());
}

void PowerControllerTest::requestsSuspendOnlyWhenAcpiSupportsIt()
{
    QString requestedAction;
    PowerController controller(
        nullptr,
        [&requestedAction](const QString &action, QString *) {
            requestedAction = action;
            return true;
        }, {},
        [](const QString &, const QStringList &arguments) {
            if (arguments.contains(QStringLiteral("hw.acpi.suspend_state"))) {
                return PowerCommandResult{true, 0, QStringLiteral("S3\nNONE\n")};
            }
            return PowerCommandResult{};
        });

    QVERIFY(controller.suspendAvailable());
    QVERIFY(controller.lidSwitchAvailable());
    QVERIFY(!controller.lidSuspendEnabled());
    QVERIFY(controller.requestSuspend());
    QCOMPARE(requestedAction, QStringLiteral("suspend"));
    QCOMPARE(controller.statusMessage(), QStringLiteral("Sleep requested"));
}

void PowerControllerTest::persistsAndConfirmsLidSleep()
{
    QString lidState = QStringLiteral("NONE");
    QStringList requestedActions;
    PowerController controller(
        nullptr,
        [&lidState, &requestedActions](const QString &action, QString *) {
            requestedActions.append(action);
            lidState = action == QStringLiteral("lid-suspend-on")
                ? QStringLiteral("S3") : QStringLiteral("NONE");
            return true;
        }, {},
        [&lidState](const QString &, const QStringList &arguments) {
            if (arguments.contains(QStringLiteral("hw.acpi.suspend_state"))) {
                return PowerCommandResult{true, 0,
                                          QStringLiteral("S3\n%1\n").arg(lidState)};
            }
            return PowerCommandResult{};
        });

    QVERIFY(controller.setLidSuspendEnabled(true));
    QVERIFY(controller.lidSuspendEnabled());
    QCOMPARE(requestedActions.constLast(), QStringLiteral("lid-suspend-on"));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("will put Northstar to sleep")));

    QVERIFY(controller.setLidSuspendEnabled(false));
    QVERIFY(!controller.lidSuspendEnabled());
    QCOMPARE(requestedActions.constLast(), QStringLiteral("lid-suspend-off"));
}

void PowerControllerTest::reportsBatteryAndAcState()
{
    QStringList calls;
    PowerController controller(
        nullptr, [](const QString &, QString *) { return true; }, {},
        [&calls](const QString &program, const QStringList &arguments) {
            calls.append(program + QLatin1Char(' ') + arguments.join(QLatin1Char(' ')));
            if (arguments.contains(QStringLiteral("hw.acpi.suspend_state"))) {
                return PowerCommandResult{};
            }
            return PowerCommandResult{true, 0, QStringLiteral("1\n64\n1\n137\n0\n")};
        });

    QVERIFY(controller.batteryAvailable());
    QCOMPARE(controller.batteryPercentage(), 64);
    QVERIFY(!controller.onAcPower());
    QVERIFY(!controller.batteryCharging());
    QCOMPARE(controller.batteryStatus(), QStringLiteral("64% - 2h 17m remaining"));
    QCOMPARE(calls.size(), 2);
    QVERIFY(calls.constFirst().contains(QStringLiteral("hw.acpi.battery.life")));
    QVERIFY(calls.constLast().contains(QStringLiteral("hw.acpi.suspend_state")));
}

void PowerControllerTest::reportsUnknownBatteryTimeHonestly()
{
    PowerController controller(
        nullptr, [](const QString &, QString *) { return true; }, {},
        [](const QString &, const QStringList &) {
            return PowerCommandResult{true, 0, QStringLiteral("1\n100\n0\n-1\n1\n")};
        });

    QVERIFY(controller.batteryAvailable());
    QVERIFY(controller.onAcPower());
    QCOMPARE(controller.batteryStatus(), QStringLiteral("Fully charged"));
    QVERIFY(!controller.batteryStatus().contains(QStringLiteral("remaining")));
}

QTEST_MAIN(PowerControllerTest)

#include "test-powercontroller.moc"
