#include "desktoplayoutcontroller.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

class DesktopLayoutControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void persistsAndLoadsPositions();
    void boundsCoordinatesAndRejectsEmptyPaths();
};

void DesktopLayoutControllerTest::persistsAndLoadsPositions()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = temporaryDirectory.filePath(QStringLiteral("desktop-layout.ini"));
    const QString itemPath = temporaryDirectory.filePath(QStringLiteral("Desktop/notes.txt"));

    DesktopLayoutController controller(nullptr, settingsPath);
    QVERIFY(controller.setPosition(itemPath, 112.0, 224.0));
    QCOMPARE(controller.positionFor(itemPath).value(QStringLiteral("x")).toReal(), 112.0);
    QCOMPARE(controller.positionFor(itemPath).value(QStringLiteral("y")).toReal(), 224.0);

    DesktopLayoutController reloaded(nullptr, settingsPath);
    QCOMPARE(reloaded.positionFor(itemPath).value(QStringLiteral("x")).toReal(), 112.0);
    QCOMPARE(reloaded.positionFor(itemPath).value(QStringLiteral("y")).toReal(), 224.0);

    QVERIFY(reloaded.clearPosition(itemPath));
    QVERIFY(reloaded.positionFor(itemPath).isEmpty());
}

void DesktopLayoutControllerTest::boundsCoordinatesAndRejectsEmptyPaths()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    DesktopLayoutController controller(nullptr, temporaryDirectory.filePath(QStringLiteral("layout.ini")));

    QVERIFY(!controller.setPosition(QString(), 10.0, 10.0));
    QVERIFY(controller.setPosition(QStringLiteral("/home/northstar/Desktop/item.txt"), -10.0, 99999.0));
    const QVariantMap position = controller.positionFor(QStringLiteral("/home/northstar/Desktop/item.txt"));
    QCOMPARE(position.value(QStringLiteral("x")).toReal(), 0.0);
    QCOMPARE(position.value(QStringLiteral("y")).toReal(), 4096.0);
}

QTEST_MAIN(DesktopLayoutControllerTest)
#include "test-desktoplayoutcontroller.moc"
