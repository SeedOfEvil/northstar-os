#include "filebrowsercontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
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
    void createsAndRenamesEntries();
    void movesEntriesToTrashWithMetadata();
    void rejectsUnsafeMutations();
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

void FileBrowserControllerTest::createsAndRenamesEntries()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(controller.createFolder(QStringLiteral("Projects")));
    QVERIFY(QFileInfo(QDir(temporaryDirectory.path()).filePath(QStringLiteral("Projects"))).isDir());

    const QString draftPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("draft.txt"));
    QVERIFY(writeFile(draftPath, "draft"));
    QVERIFY(controller.renameEntry(draftPath, QStringLiteral("notes.txt")));
    QVERIFY(!QFileInfo::exists(draftPath));
    QVERIFY(QFileInfo::exists(QDir(temporaryDirectory.path()).filePath(QStringLiteral("notes.txt"))));
}

void FileBrowserControllerTest::movesEntriesToTrashWithMetadata()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString sourcePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("remove-me.txt"));
    QVERIFY(writeFile(sourcePath, "remove"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(controller.moveToTrash(sourcePath));
    QVERIFY(!QFileInfo::exists(sourcePath));

    const QString trashFilesPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral(".local/share/Trash/files"));
    const QString trashInfoPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral(".local/share/Trash/info"));
    const QString trashedPath = QDir(trashFilesPath).filePath(QStringLiteral("remove-me.txt"));
    const QString metadataPath = QDir(trashInfoPath).filePath(QStringLiteral("remove-me.txt.trashinfo"));
    QVERIFY(QFileInfo::exists(trashedPath));
    QVERIFY(QFileInfo::exists(metadataPath));

    QFile metadata(metadataPath);
    QVERIFY(metadata.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray metadataContents = metadata.readAll();
    QVERIFY(metadataContents.contains("[Trash Info]"));
    QVERIFY(metadataContents.contains(QUrl::toPercentEncoding(QFileInfo(sourcePath).absoluteFilePath())));
}

void FileBrowserControllerTest::rejectsUnsafeMutations()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourcePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("notes.txt"));
    QVERIFY(writeFile(sourcePath, "notes"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(!controller.createFolder(QStringLiteral("../outside")));
    QVERIFY(!controller.renameEntry(sourcePath, QStringLiteral("../outside")));
    QVERIFY(!controller.moveToTrash(temporaryDirectory.path()));
    QVERIFY(QFileInfo::exists(sourcePath));
}

QTEST_MAIN(FileBrowserControllerTest)
#include "test-filebrowsercontroller.moc"
