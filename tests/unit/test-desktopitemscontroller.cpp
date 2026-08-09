#include "desktopitemscontroller.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class DesktopItemsControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void listsDesktopItemsWithStableMetadata();
    void refreshesWhenDesktopFolderIsCreated();
    void emitsSafeOpenRequests();
};

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

QVariantMap entryAt(const DesktopItemsController &controller, int index)
{
    return controller.entries().at(index).toMap();
}

} // namespace

void DesktopItemsControllerTest::listsDesktopItemsWithStableMetadata()
{
    QTemporaryDir homeDirectory;
    QVERIFY(homeDirectory.isValid());
    const QString desktopPath = QDir(homeDirectory.path()).filePath(QStringLiteral("Desktop"));
    QVERIFY(QDir().mkpath(QDir(desktopPath).filePath(QStringLiteral("Documents"))));
    QVERIFY(writeFile(QDir(desktopPath).filePath(QStringLiteral("notes.txt")), "Northstar"));
    QVERIFY(writeFile(QDir(desktopPath).filePath(QStringLiteral("Northstar.desktop")), "[Desktop Entry]\n"));
    QVERIFY(writeFile(QDir(desktopPath).filePath(QStringLiteral(".hidden")), "hidden"));

    DesktopItemsController controller(nullptr, homeDirectory.path());

    QVERIFY(controller.available());
    QCOMPARE(controller.entries().size(), 3);
    QCOMPARE(entryAt(controller, 0).value(QStringLiteral("name")).toString(), QStringLiteral("Documents"));
    QVERIFY(entryAt(controller, 0).value(QStringLiteral("isDirectory")).toBool());
    QCOMPARE(entryAt(controller, 1).value(QStringLiteral("name")).toString(), QStringLiteral("notes.txt"));
    QVERIFY(!entryAt(controller, 1).value(QStringLiteral("isDirectory")).toBool());
    QVERIFY(!entryAt(controller, 1).value(QStringLiteral("isLaunchable")).toBool());
    QCOMPARE(entryAt(controller, 2).value(QStringLiteral("kind")).toString(), QStringLiteral("Application"));
    QVERIFY(entryAt(controller, 2).value(QStringLiteral("isLaunchable")).toBool());
    QCOMPARE(entryAt(controller, 2).value(QStringLiteral("path")).toString(),
             QFileInfo(QDir(desktopPath).filePath(QStringLiteral("Northstar.desktop"))).canonicalFilePath());
}

void DesktopItemsControllerTest::refreshesWhenDesktopFolderIsCreated()
{
    QTemporaryDir homeDirectory;
    QVERIFY(homeDirectory.isValid());

    DesktopItemsController controller(nullptr, homeDirectory.path());
    QVERIFY(!controller.available());
    QVERIFY(controller.entries().isEmpty());

    const QString desktopPath = QDir(homeDirectory.path()).filePath(QStringLiteral("Desktop"));
    QVERIFY(QDir().mkpath(desktopPath));
    QVERIFY(writeFile(QDir(desktopPath).filePath(QStringLiteral("first.txt")), "first"));
    controller.refresh();

    QVERIFY(controller.available());
    QCOMPARE(controller.entries().size(), 1);
}

void DesktopItemsControllerTest::emitsSafeOpenRequests()
{
    QTemporaryDir homeDirectory;
    QVERIFY(homeDirectory.isValid());
    const QString desktopPath = QDir(homeDirectory.path()).filePath(QStringLiteral("Desktop"));
    QVERIFY(QDir().mkpath(desktopPath));
    const QString filePath = QDir(desktopPath).filePath(QStringLiteral("notes.txt"));
    const QString folderPath = QDir(desktopPath).filePath(QStringLiteral("Documents"));
    QVERIFY(writeFile(filePath, "notes"));
    QVERIFY(QDir().mkpath(folderPath));

    DesktopItemsController controller(nullptr, homeDirectory.path());
    QSignalSpy openSpy(&controller, &DesktopItemsController::openPathRequested);
    QSignalSpy openWithSpy(&controller, &DesktopItemsController::openWithRequested);

    QVERIFY(controller.requestOpen(filePath));
    QCOMPARE(openSpy.count(), 1);
    QCOMPARE(openSpy.at(0).at(0).toString(), QFileInfo(filePath).canonicalFilePath());
    QVERIFY(!openSpy.at(0).at(1).toBool());
    QVERIFY(!openSpy.at(0).at(2).toBool());

    QVERIFY(controller.requestOpen(folderPath));
    QCOMPARE(openSpy.count(), 2);
    QVERIFY(openSpy.at(1).at(1).toBool());

    QVERIFY(controller.requestOpenWith(filePath));
    QCOMPARE(openWithSpy.count(), 1);
    QCOMPARE(openWithSpy.at(0).at(0).toString(), QFileInfo(filePath).canonicalFilePath());

    QVERIFY(!controller.requestOpen(QDir(homeDirectory.path()).filePath(QStringLiteral("../outside"))));
    QCOMPARE(openSpy.count(), 2);
}

QTEST_MAIN(DesktopItemsControllerTest)
#include "test-desktopitemscontroller.moc"
