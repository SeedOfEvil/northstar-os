#include "powercontroller.h"

#include <QtTest/QtTest>

class PowerControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void requestsRestartThroughControlledAction();
    void reportsPowerActionFailure();
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

void PowerControllerTest::reportsBatteryAndAcState()
{
    QStringList calls;
    PowerController controller(
        nullptr, [](const QString &, QString *) { return true; }, {},
        [&calls](const QString &program, const QStringList &arguments) {
            calls.append(program + QLatin1Char(' ') + arguments.join(QLatin1Char(' ')));
            return PowerCommandResult{true, 0, QStringLiteral("1\n64\n1\n137\n0\n")};
        });

    QVERIFY(controller.batteryAvailable());
    QCOMPARE(controller.batteryPercentage(), 64);
    QVERIFY(!controller.onAcPower());
    QVERIFY(!controller.batteryCharging());
    QCOMPARE(controller.batteryStatus(), QStringLiteral("64% - 2h 17m remaining"));
    QCOMPARE(calls.size(), 1);
    QVERIFY(calls.constFirst().contains(QStringLiteral("hw.acpi.battery.life")));
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
