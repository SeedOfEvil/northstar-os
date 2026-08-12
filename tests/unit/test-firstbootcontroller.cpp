#include "firstbootcontroller.h"

#include <QtTest>

class TestFirstBootController final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsSupportedProfile();
    void rejectsUnsafeProfiles();
    void exposesBoundedRegionalChoices();
};

void TestFirstBootController::acceptsSupportedProfile()
{
    FirstBootController controller;
    QCOMPARE(controller.validateProfile(QStringLiteral("Hector Northstar"),
                                        QStringLiteral("hector"),
                                        QStringLiteral("correct-horse"),
                                        QStringLiteral("correct-horse"),
                                        QStringLiteral("en_US.UTF-8"),
                                        QStringLiteral("America/Denver"),
                                        QStringLiteral("us")),
             QString());
}

void TestFirstBootController::rejectsUnsafeProfiles()
{
    FirstBootController controller;
    QVERIFY(!controller.validateProfile(QStringLiteral("Hector"), QStringLiteral("Root User"),
                                        QStringLiteral("correct-horse"), QStringLiteral("correct-horse"),
                                        QStringLiteral("en_US.UTF-8"), QStringLiteral("America/Denver"),
                                        QStringLiteral("us")).isEmpty());
    QVERIFY(!controller.validateProfile(QStringLiteral("Hector"), QStringLiteral("hector"),
                                        QStringLiteral("short"), QStringLiteral("short"),
                                        QStringLiteral("en_US.UTF-8"), QStringLiteral("America/Denver"),
                                        QStringLiteral("us")).isEmpty());
    QVERIFY(!controller.validateProfile(QStringLiteral("Hector"), QStringLiteral("hector"),
                                        QStringLiteral("correct-horse"), QStringLiteral("different-pass"),
                                        QStringLiteral("en_US.UTF-8"), QStringLiteral("America/Denver"),
                                        QStringLiteral("us")).isEmpty());
    QVERIFY(!controller.validateProfile(QStringLiteral("Hector"), QStringLiteral("hector"),
                                        QStringLiteral("correct-horse"), QStringLiteral("correct-horse"),
                                        QStringLiteral("../../etc"), QStringLiteral("../../zone"),
                                        QStringLiteral("custom;cmd")).isEmpty());
}

void TestFirstBootController::exposesBoundedRegionalChoices()
{
    FirstBootController controller;
    QVERIFY(controller.locales().contains(QStringLiteral("en_US.UTF-8")));
    QVERIFY(controller.timezones().contains(QStringLiteral("America/Denver")));
    QVERIFY(controller.keyboardLayouts().contains(QStringLiteral("us")));
    QVERIFY(controller.locales().size() <= 12);
    QVERIFY(controller.timezones().size() <= 20);
    QVERIFY(controller.keyboardLayouts().size() <= 12);
}

QTEST_MAIN(TestFirstBootController)
#include "test-firstbootcontroller.moc"
