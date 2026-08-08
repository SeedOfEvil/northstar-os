#include "filebrowsercontroller.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class FileBrowserControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void listsFoldersBeforeFiles();
    void navigatesWithinHomeFolder();
    void opensFilesThroughInjectedHandler();
    void rejectsPathsOutsideHomeFolder();
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

QString entryName(const QVariant &entry)
{
    return entry.toMap().value(QStringLiteral("name")).toString();
}

} // namespace

void FileBrowserControllerTest::listsFoldersBeforeFiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(QDir(temporaryDirectory.path()).mkdir(QStringLiteral("Documents")));
    QVERIFY(writeFile(QDir(temporaryDirectory.path()).filePath(QStringLiteral("z-last.txt")), "z"));
    QVERIFY(writeFile(QDir(temporaryDirectory.path()).filePath(QStringLiteral("a-first.txt")), "a"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    const QVariantList entries = controller.entries();

    QCOMPARE(entries.size(), 3);
    QCOMPARE(entryName(entries.at(0)), QStringLiteral("Documents"));
    QCOMPARE(entryName(entries.at(1)), QStringLiteral("a-first.txt"));
    QCOMPARE(entryName(entries.at(2)), QStringLiteral("z-last.txt"));
    QVERIFY(entries.at(0).toMap().value(QStringLiteral("isDirectory")).toBool());
    QVERIFY(!entries.at(1).toMap().value(QStringLiteral("isDirectory")).toBool());
}

void FileBrowserControllerTest::navigatesWithinHomeFolder()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(QDir(temporaryDirectory.path()).mkdir(QStringLiteral("Documents")));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(controller.navigateTo(QDir(temporaryDirectory.path()).filePath(QStringLiteral("Documents"))));
    QCOMPARE(controller.displayPath(), QStringLiteral("~/Documents"));
    QVERIFY(controller.navigateUp());
    QCOMPARE(controller.currentPath(), QDir::cleanPath(QDir::fromNativeSeparators(temporaryDirectory.path())));
    QVERIFY(!controller.navigateUp());
}

void FileBrowserControllerTest::opensFilesThroughInjectedHandler()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString filePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("readme.txt"));
    QVERIFY(writeFile(filePath, "Northstar"));

    QUrl openedUrl;
    FileBrowserController controller(
        nullptr,
        temporaryDirectory.path(),
        [&openedUrl](const QUrl &url) {
            openedUrl = url;
            return true;
        });

    QVERIFY(controller.openEntry(filePath));
    QCOMPARE(openedUrl, QUrl::fromLocalFile(QFileInfo(filePath).canonicalFilePath()));
    QVERIFY(controller.errorMessage().isEmpty());
}

void FileBrowserControllerTest::rejectsPathsOutsideHomeFolder()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(!controller.navigateTo(QDir(temporaryDirectory.path()).filePath(QStringLiteral(".."))));
    QVERIFY(!controller.errorMessage().isEmpty());
    QCOMPARE(controller.currentPath(), QDir::cleanPath(QDir::fromNativeSeparators(temporaryDirectory.path())));
}

QTEST_MAIN(FileBrowserControllerTest)
#include "test-filebrowsercontroller.moc"
