#include "powercontroller.h"

#include <QtTest/QtTest>

class PowerControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void requestsRestartThroughControlledAction();
    void reportsPowerActionFailure();
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

QTEST_MAIN(PowerControllerTest)

#include "test-powercontroller.moc"
