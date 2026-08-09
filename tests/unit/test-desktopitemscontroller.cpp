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
    void watchesDesktopFolderChanges();
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

QVariantMap entryNamed(const DesktopItemsController &controller, const QString &name)
{
    for (const QVariant &entry : controller.entries()) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("name")).toString() == name) {
            return map;
        }
    }
    return {};
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
    const QVariantMap notesEntry = entryNamed(controller, QStringLiteral("notes.txt"));
    QVERIFY(!notesEntry.isEmpty());
    QVERIFY(!notesEntry.value(QStringLiteral("isDirectory")).toBool());
    QVERIFY(!notesEntry.value(QStringLiteral("isLaunchable")).toBool());

    const QVariantMap applicationEntry = entryNamed(controller, QStringLiteral("Northstar.desktop"));
    QCOMPARE(applicationEntry.value(QStringLiteral("kind")).toString(), QStringLiteral("Application"));
    QVERIFY(applicationEntry.value(QStringLiteral("isLaunchable")).toBool());
    QCOMPARE(applicationEntry.value(QStringLiteral("path")).toString(),
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

void DesktopItemsControllerTest::watchesDesktopFolderChanges()
{
    QTemporaryDir homeDirectory;
    QVERIFY(homeDirectory.isValid());
    const QString desktopPath = QDir(homeDirectory.path()).filePath(QStringLiteral("Desktop"));
    QVERIFY(QDir().mkpath(desktopPath));

    DesktopItemsController controller(nullptr, homeDirectory.path());
    QVERIFY(controller.entries().isEmpty());

    QVERIFY(writeFile(QDir(desktopPath).filePath(QStringLiteral("watched.txt")), "watched"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.entries().size() == 1, 3000);

    QVERIFY(QFile::remove(QDir(desktopPath).filePath(QStringLiteral("watched.txt"))));
    QTRY_VERIFY_WITH_TIMEOUT(controller.entries().isEmpty(), 3000);
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
