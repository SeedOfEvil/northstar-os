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
    void searchesHomeTreeAndClearsOnNavigation();
    void opensFilesThroughInjectedHandler();
    void rejectsPathsOutsideHomeFolder();
    void createsAndRenamesEntries();
    void createsFiles();
    void movesEntriesToTrashWithMetadata();
    void showsAndRestoresTrashEntries();
    void emptiesTrash();
    void rejectsUnsafeMutations();
    void opensMountedLocationReadOnly();
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

void FileBrowserControllerTest::searchesHomeTreeAndClearsOnNavigation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString documentsPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Documents/Projects"));
    QVERIFY(QDir().mkpath(documentsPath));
    QVERIFY(writeFile(QDir(documentsPath).filePath(QStringLiteral("notes.txt")), "Northstar"));
    QVERIFY(writeFile(QDir(temporaryDirectory.path()).filePath(QStringLiteral("readme.txt")), "root"));

    const QString trashPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral(".local/share/Trash/files"));
    QVERIFY(QDir().mkpath(trashPath));
    QVERIFY(writeFile(QDir(trashPath).filePath(QStringLiteral("deleted-notes.txt")), "trash"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    controller.setSearchQuery(QStringLiteral("notes"));

    QVERIFY(controller.searching());
    QCOMPARE(controller.displayPath(), QStringLiteral("Search: notes"));
    QCOMPARE(controller.entries().size(), 1);
    QCOMPARE(entryName(controller.entries().first()), QStringLiteral("notes.txt"));
    QVERIFY(controller.entries().first().toMap().value(QStringLiteral("searchLocation")).toString()
                .endsWith(QStringLiteral("Documents/Projects/notes.txt")));

    controller.setSearchQuery(QStringLiteral("deleted"));
    QVERIFY(controller.entries().isEmpty());

    QVERIFY(controller.navigateTo(QDir(temporaryDirectory.path()).filePath(QStringLiteral("Documents"))));
    QVERIFY(!controller.searching());
    QVERIFY(controller.searchQuery().isEmpty());
    QCOMPARE(controller.displayPath(), QStringLiteral("~/Documents"));
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

void FileBrowserControllerTest::createsFiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(controller.createFile(QStringLiteral("association-test.txt")));

    const QString filePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("association-test.txt"));
    QVERIFY(QFileInfo(filePath).isFile());
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray());

    QVERIFY(!controller.createFile(QStringLiteral("../outside.txt")));
    QVERIFY(!controller.createFile(QStringLiteral("association-test.txt")));
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

void FileBrowserControllerTest::showsAndRestoresTrashEntries()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourcePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("restore-me.txt"));
    QVERIFY(writeFile(sourcePath, "restore"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(controller.moveToTrash(sourcePath));
    QVERIFY(controller.showTrash());
    QVERIFY(controller.showingTrash());
    QCOMPARE(controller.displayPath(), QStringLiteral("Trash"));
    QCOMPARE(controller.entries().size(), 1);

    const QVariantMap entry = controller.entries().first().toMap();
    QVERIFY(entry.value(QStringLiteral("isTrashEntry")).toBool());
    QCOMPARE(entry.value(QStringLiteral("originalPath")).toString(),
             QDir::cleanPath(QDir::fromNativeSeparators(sourcePath)));

    QVERIFY(controller.restoreEntry(entry.value(QStringLiteral("path")).toString()));
    QVERIFY(QFileInfo::exists(sourcePath));
    QVERIFY(!QFileInfo::exists(QDir(temporaryDirectory.path())
                                   .filePath(QStringLiteral(".local/share/Trash/info/restore-me.txt.trashinfo"))));
    QVERIFY(controller.entries().isEmpty());
}

void FileBrowserControllerTest::emptiesTrash()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(writeFile(QDir(temporaryDirectory.path()).filePath(QStringLiteral("one.txt")), "one"));
    QVERIFY(writeFile(QDir(temporaryDirectory.path()).filePath(QStringLiteral("two.txt")), "two"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(controller.moveToTrash(QDir(temporaryDirectory.path()).filePath(QStringLiteral("one.txt"))));
    QVERIFY(controller.moveToTrash(QDir(temporaryDirectory.path()).filePath(QStringLiteral("two.txt"))));
    QVERIFY(controller.showTrash());
    QCOMPARE(controller.entries().size(), 2);
    QVERIFY(controller.emptyTrash());
    QVERIFY(controller.entries().isEmpty());
    QVERIFY(QDir(QDir(temporaryDirectory.path()).filePath(QStringLiteral(".local/share/Trash/files")))
                .entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
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

void FileBrowserControllerTest::opensMountedLocationReadOnly()
{
    QTemporaryDir homeDirectory;
    QTemporaryDir volumeDirectory;
    QVERIFY(homeDirectory.isValid());
    QVERIFY(volumeDirectory.isValid());

    const QString documentsPath = QDir(volumeDirectory.path()).filePath(QStringLiteral("Documents"));
    QVERIFY(QDir().mkpath(documentsPath));
    const QString notePath = QDir(documentsPath).filePath(QStringLiteral("note.txt"));
    QVERIFY(writeFile(notePath, "mounted"));

    FileBrowserController controller(nullptr, homeDirectory.path(), {}, {volumeDirectory.path()});
    QVERIFY(controller.openLocation(volumeDirectory.path(), QStringLiteral("Test Volume")));
    QVERIFY(!controller.homeLocation());
    QVERIFY(controller.readOnlyLocation());
    QVERIFY(!controller.canNavigateUp());
    QCOMPARE(controller.locationRoot(), QFileInfo(volumeDirectory.path()).canonicalFilePath());
    QCOMPARE(controller.displayPath(),
             QStringLiteral("Test Volume (%1)").arg(QFileInfo(volumeDirectory.path()).canonicalFilePath()));
    QVERIFY(controller.entries().size() >= 1);
    QVERIFY(controller.entries().first().toMap().value(QStringLiteral("readOnly")).toBool());

    QVERIFY(controller.navigateTo(documentsPath));
    QVERIFY(controller.canNavigateUp());
    QVERIFY(!controller.createFile(QStringLiteral("blocked.txt")));
    QVERIFY(controller.errorMessage().contains(QStringLiteral("Home")));
    QVERIFY(!QFileInfo::exists(QDir(documentsPath).filePath(QStringLiteral("blocked.txt"))));

    QVERIFY(controller.goHome());
    QVERIFY(controller.homeLocation());
    QVERIFY(!controller.readOnlyLocation());
    QVERIFY(!controller.canNavigateUp());
}

QTEST_MAIN(FileBrowserControllerTest)
#include "test-filebrowsercontroller.moc"
