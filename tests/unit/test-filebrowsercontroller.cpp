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
    void sortsEntriesByMetadata();
    void listsDesktopEntriesForTheDesktopSurface();
    void classifiesLaunchableDesktopEntries();
    void watchesForDesktopFolderCreation();
    void navigatesWithinHomeFolder();
    void searchesHomeTreeAndClearsOnNavigation();
    void opensFilesThroughInjectedHandler();
    void rejectsPathsOutsideHomeFolder();
    void resolvesSafeHomeChildPaths();
    void createsAndRenamesEntries();
    void createsFiles();
    void movesEntriesToTrashWithMetadata();
    void showsAndRestoresTrashEntries();
    void emptiesTrash();
    void copiesAndUndoesEntries();
    void movesAndUndoesEntries();
    void resolvesPasteConflictsWithKeepBoth();
    void copiesFromMountedLocationButRejectsCut();
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

void FileBrowserControllerTest::sortsEntriesByMetadata()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(QDir(temporaryDirectory.path()).mkdir(QStringLiteral("folder")));
    QVERIFY(writeFile(QDir(temporaryDirectory.path()).filePath(QStringLiteral("small.txt")), "1"));
    QVERIFY(writeFile(QDir(temporaryDirectory.path()).filePath(QStringLiteral("large.txt")), "1234"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    controller.setSortMode(QStringLiteral("size"));
    QCOMPARE(entryName(controller.entries().at(0)), QStringLiteral("folder"));
    QCOMPARE(entryName(controller.entries().at(1)), QStringLiteral("small.txt"));
    QCOMPARE(entryName(controller.entries().at(2)), QStringLiteral("large.txt"));

    controller.toggleSortOrder();
    QCOMPARE(controller.sortMode(), QStringLiteral("size"));
    QVERIFY(!controller.sortAscending());
    QCOMPARE(entryName(controller.entries().at(0)), QStringLiteral("folder"));
    QCOMPARE(entryName(controller.entries().at(1)), QStringLiteral("large.txt"));
    QCOMPARE(entryName(controller.entries().at(2)), QStringLiteral("small.txt"));

    controller.setSortMode(QStringLiteral("type"));
    QCOMPARE(controller.sortMode(), QStringLiteral("type"));
    QVERIFY(!controller.sortAscending());
    controller.setSortMode(QStringLiteral("unsafe"));
    QCOMPARE(controller.sortMode(), QStringLiteral("type"));
}

void FileBrowserControllerTest::listsDesktopEntriesForTheDesktopSurface()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString desktopPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Desktop"));
    QVERIFY(QDir().mkpath(desktopPath));
    QVERIFY(QDir(desktopPath).mkdir(QStringLiteral("Projects")));
    QVERIFY(writeFile(QDir(desktopPath).filePath(QStringLiteral("notes.txt")), "notes"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    const QVariantList entries = controller.desktopEntries();

    QCOMPARE(entries.size(), 2);
    QCOMPARE(entryName(entries.at(0)), QStringLiteral("Projects"));
    QCOMPARE(entryName(entries.at(1)), QStringLiteral("notes.txt"));
    QCOMPARE(entries.at(0).toMap().value(QStringLiteral("path")).toString(),
             QFileInfo(QDir(desktopPath).filePath(QStringLiteral("Projects"))).canonicalFilePath());
    QVERIFY(entries.at(0).toMap().value(QStringLiteral("isDirectory")).toBool());
    QVERIFY(!entries.at(1).toMap().value(QStringLiteral("isDirectory")).toBool());
}

void FileBrowserControllerTest::classifiesLaunchableDesktopEntries()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString desktopPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Desktop"));
    QVERIFY(QDir().mkpath(desktopPath));
    QVERIFY(writeFile(QDir(desktopPath).filePath(QStringLiteral("Northstar.desktop")),
                      "[Desktop Entry]\nName=Northstar\nType=Application\n"));
    QVERIFY(QDir(desktopPath).mkdir(QStringLiteral("Example.app")));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    const QVariantList entries = controller.desktopEntries();
    QCOMPARE(entries.size(), 2);

    QVariantMap desktopFile;
    QVariantMap appDirectory;
    for (const QVariant &entry : entries) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("name")).toString() == QStringLiteral("Northstar.desktop")) {
            desktopFile = map;
        } else if (map.value(QStringLiteral("name")).toString() == QStringLiteral("Example.app")) {
            appDirectory = map;
        }
    }
    QVERIFY(!desktopFile.isEmpty());
    QVERIFY(!appDirectory.isEmpty());
    QCOMPARE(desktopFile.value(QStringLiteral("name")).toString(),
             QStringLiteral("Northstar.desktop"));
    QVERIFY(desktopFile.value(QStringLiteral("isLaunchable")).toBool());
    QVERIFY(!desktopFile.value(QStringLiteral("isDirectory")).toBool());
    QCOMPARE(desktopFile.value(QStringLiteral("kind")).toString(),
             QStringLiteral("Application"));
    QCOMPARE(appDirectory.value(QStringLiteral("name")).toString(),
             QStringLiteral("Example.app"));
    QVERIFY(appDirectory.value(QStringLiteral("isLaunchable")).toBool());
    QVERIFY(!appDirectory.value(QStringLiteral("isDirectory")).toBool());
    QCOMPARE(appDirectory.value(QStringLiteral("kind")).toString(),
             QStringLiteral("Application"));
}

void FileBrowserControllerTest::watchesForDesktopFolderCreation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(controller.desktopEntries().isEmpty());

    const QString desktopPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Desktop"));
    QVERIFY(QDir().mkpath(desktopPath));
    QVERIFY(writeFile(QDir(desktopPath).filePath(QStringLiteral("created-later.txt")), "later"));

    QTRY_COMPARE_WITH_TIMEOUT(controller.desktopEntries().size(), 1, 2000);
    QCOMPARE(entryName(controller.desktopEntries().first()), QStringLiteral("created-later.txt"));
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

void FileBrowserControllerTest::resolvesSafeHomeChildPaths()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(QDir(temporaryDirectory.path()).mkdir(QStringLiteral("Desktop")));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QCOMPARE(controller.homeChildPath(QStringLiteral("Desktop")),
             QFileInfo(QDir(temporaryDirectory.path()).filePath(QStringLiteral("Desktop"))).canonicalFilePath());
    QVERIFY(controller.homeChildPath(QStringLiteral("Missing")).isEmpty());
    QVERIFY(controller.homeChildPath(QStringLiteral("../outside")).isEmpty());
    QVERIFY(controller.homeChildPath(QDir(temporaryDirectory.path()).filePath(QStringLiteral("Desktop"))).isEmpty());
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

void FileBrowserControllerTest::copiesAndUndoesEntries()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourceDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Source"));
    const QString destinationDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Destination"));
    QVERIFY(QDir().mkpath(sourceDirectory));
    QVERIFY(QDir().mkpath(destinationDirectory));
    QVERIFY(writeFile(QDir(sourceDirectory).filePath(QStringLiteral("notes.txt")), "Northstar"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(controller.copyEntry(sourceDirectory));
    QCOMPARE(controller.clipboardOperation(), QStringLiteral("copy"));
    QVERIFY(controller.navigateTo(destinationDirectory));
    QVERIFY(controller.canPaste());
    QVERIFY(controller.pasteClipboard());

    const QString copiedDirectory = QDir(destinationDirectory).filePath(QStringLiteral("Source"));
    QVERIFY(QFileInfo::exists(QDir(copiedDirectory).filePath(QStringLiteral("notes.txt"))));
    QVERIFY(controller.canUndo());
    QCOMPARE(controller.transferProgress(), 100);
    QVERIFY(controller.undoLastTransfer());
    QVERIFY(!QFileInfo::exists(copiedDirectory));
    QVERIFY(!controller.canUndo());
    QVERIFY(QFileInfo::exists(QDir(sourceDirectory).filePath(QStringLiteral("notes.txt"))));
}

void FileBrowserControllerTest::movesAndUndoesEntries()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString destinationDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Destination"));
    const QString sourcePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("move-me.txt"));
    QVERIFY(QDir().mkpath(destinationDirectory));
    QVERIFY(writeFile(sourcePath, "move"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(controller.cutEntry(sourcePath));
    QCOMPARE(controller.clipboardOperation(), QStringLiteral("cut"));
    QVERIFY(controller.navigateTo(destinationDirectory));
    QVERIFY(controller.pasteClipboard());
    const QString movedPath = QDir(destinationDirectory).filePath(QStringLiteral("move-me.txt"));
    QVERIFY(!QFileInfo::exists(sourcePath));
    QVERIFY(QFileInfo::exists(movedPath));
    QVERIFY(controller.clipboardOperation().isEmpty());
    QCOMPARE(controller.undoLabel(), QStringLiteral("Undo move"));

    QVERIFY(controller.undoLastTransfer());
    QVERIFY(QFileInfo::exists(sourcePath));
    QVERIFY(!QFileInfo::exists(movedPath));
}

void FileBrowserControllerTest::resolvesPasteConflictsWithKeepBoth()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourceDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Source"));
    const QString destinationDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("Destination"));
    QVERIFY(QDir().mkpath(sourceDirectory));
    QVERIFY(QDir().mkpath(destinationDirectory));
    const QString sourcePath = QDir(sourceDirectory).filePath(QStringLiteral("notes.txt"));
    QVERIFY(writeFile(sourcePath, "new"));
    QVERIFY(writeFile(QDir(destinationDirectory).filePath(QStringLiteral("notes.txt")), "old"));

    FileBrowserController controller(nullptr, temporaryDirectory.path());
    QVERIFY(controller.copyEntry(sourcePath));
    QVERIFY(controller.navigateTo(destinationDirectory));
    QVERIFY(!controller.pasteClipboard());
    QVERIFY(controller.conflictPending());
    QCOMPARE(controller.conflictName(), QStringLiteral("notes.txt"));
    QVERIFY(controller.pasteClipboard(QStringLiteral("keepBoth")));
    QVERIFY(!controller.conflictPending());
    QVERIFY(QFileInfo::exists(QDir(destinationDirectory).filePath(QStringLiteral("notes copy.txt"))));
}

void FileBrowserControllerTest::copiesFromMountedLocationButRejectsCut()
{
    QTemporaryDir homeDirectory;
    QTemporaryDir volumeDirectory;
    QVERIFY(homeDirectory.isValid());
    QVERIFY(volumeDirectory.isValid());
    const QString sourcePath = QDir(volumeDirectory.path()).filePath(QStringLiteral("volume-note.txt"));
    QVERIFY(writeFile(sourcePath, "mounted"));

    FileBrowserController controller(nullptr, homeDirectory.path(), {}, {volumeDirectory.path()});
    QVERIFY(controller.openLocation(volumeDirectory.path(), QStringLiteral("Volume")));
    QVERIFY(controller.copyEntry(sourcePath));
    QVERIFY(!controller.cutEntry(sourcePath));
    QVERIFY(controller.goHome());
    QVERIFY(controller.pasteClipboard());
    QVERIFY(QFileInfo::exists(QDir(homeDirectory.path()).filePath(QStringLiteral("volume-note.txt"))));
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
