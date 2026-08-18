#include "recentfilesstore.h"
#include "texteditorcontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TextEditorControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void startsWithOneEmptyDocument();
    void reusesThePristineTabForTheFirstFile();
    void keepsDocumentsIndependent();
    void activatesADocumentThatIsAlreadyOpen();
    void refusesToCloseAnUnsavedDocument();
    void alwaysKeepsOneDocumentOpen();
    void savesNewDocumentsWithSaveAs();
    void refusesSaveAsOverAnOpenDocument();
    void reportsMissingUnreadableOversizedAndBinaryFiles();
    void recreatesADocumentRemovedOnDisk();
    void refusesToOverwriteExternalChangesUntilConfirmed();
    void reloadsExternalChanges();
    void preservesFilePermissionsOnSave();
    void findsMatchesInBothDirections();
    void honoursCaseSensitivity();
    void replacesTheSelectedMatchAndAllMatches();
    void browsesDirectoriesForOpening();
    void persistsRecentFilesAcrossSessions();
    void dropsRecentEntriesOnRequest();

private:
    QString path(const QString &name) const;
    QString writeFile(const QString &name, const QByteArray &contents) const;
    QString recentPath() const;

    QTemporaryDir *m_directory = nullptr;
};

void TextEditorControllerTest::init()
{
    m_directory = new QTemporaryDir;
    QVERIFY(m_directory->isValid());
}

void TextEditorControllerTest::cleanup()
{
    delete m_directory;
    m_directory = nullptr;
}

QString TextEditorControllerTest::path(const QString &name) const
{
    return m_directory->filePath(name);
}

QString TextEditorControllerTest::writeFile(const QString &name, const QByteArray &contents) const
{
    const QString filePath = path(name);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()) {
        return {};
    }
    file.close();
    return filePath;
}

QString TextEditorControllerTest::recentPath() const
{
    return path(QStringLiteral("recent.ini"));
}

// --- Documents and tabs ----------------------------------------------------

void TextEditorControllerTest::startsWithOneEmptyDocument()
{
    TextEditorController controller(nullptr, recentPath());
    QCOMPARE(controller.documentCount(), 1);
    QCOMPARE(controller.activeIndex(), 0);
    QVERIFY(controller.untitled());
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.anyDirty());
    QCOMPARE(controller.documentTitle(), QStringLiteral("Untitled"));
    QCOMPARE(controller.displayPath(), QStringLiteral("Not saved yet"));
}

void TextEditorControllerTest::reusesThePristineTabForTheFirstFile()
{
    const QString notes = writeFile(QStringLiteral("notes.txt"), "Northstar");
    QVERIFY(!notes.isEmpty());

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.openFile(notes));
    QCOMPARE(controller.documentCount(), 1);
    QCOMPARE(controller.text(), QStringLiteral("Northstar"));
    QCOMPARE(controller.documentTitle(), QStringLiteral("notes.txt"));
    QVERIFY(!controller.untitled());

    // A second file opens beside the first instead of replacing it.
    const QString other = writeFile(QStringLiteral("other.txt"), "Lunar");
    QVERIFY(controller.openFile(other));
    QCOMPARE(controller.documentCount(), 2);
    QCOMPARE(controller.activeIndex(), 1);
}

void TextEditorControllerTest::keepsDocumentsIndependent()
{
    const QString first = writeFile(QStringLiteral("first.txt"), "one");
    const QString second = writeFile(QStringLiteral("second.txt"), "two");

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.openFile(first));
    QVERIFY(controller.openFile(second));

    controller.setText(QStringLiteral("two edited"));
    QVERIFY(controller.dirty());
    QCOMPARE(controller.firstDirtyIndex(), 1);

    controller.activateDocument(0);
    QCOMPARE(controller.text(), QStringLiteral("one"));
    QVERIFY(!controller.dirty());
    QVERIFY(controller.anyDirty());

    controller.activateDocument(1);
    QCOMPARE(controller.text(), QStringLiteral("two edited"));

    QVERIFY(controller.save());
    QVERIFY(!controller.anyDirty());

    QFile saved(second);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(saved.readAll(), QByteArray("two edited"));
}

void TextEditorControllerTest::activatesADocumentThatIsAlreadyOpen()
{
    const QString first = writeFile(QStringLiteral("first.txt"), "one");
    const QString second = writeFile(QStringLiteral("second.txt"), "two");

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.openFile(first));
    QVERIFY(controller.openFile(second));
    QCOMPARE(controller.activeIndex(), 1);

    QVERIFY(controller.openFile(first));
    QCOMPARE(controller.documentCount(), 2);
    QCOMPARE(controller.activeIndex(), 0);
    QVERIFY(controller.statusMessage().contains(QStringLiteral("already open")));
}

void TextEditorControllerTest::refusesToCloseAnUnsavedDocument()
{
    const QString first = writeFile(QStringLiteral("first.txt"), "one");
    const QString second = writeFile(QStringLiteral("second.txt"), "two");

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.openFile(first));
    QVERIFY(controller.openFile(second));
    controller.setText(QStringLiteral("two edited"));

    QVERIFY(!controller.closeDocument(1));
    QVERIFY(controller.statusIsError());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("unsaved changes")));
    QCOMPARE(controller.documentCount(), 2);

    controller.discardDocument(1);
    QCOMPARE(controller.documentCount(), 1);
    QCOMPARE(controller.activeIndex(), 0);
    QCOMPARE(controller.text(), QStringLiteral("one"));
    QVERIFY(!controller.anyDirty());

    // The untouched document still closes without a confirmation.
    QVERIFY(controller.closeDocument(0));
}

void TextEditorControllerTest::alwaysKeepsOneDocumentOpen()
{
    TextEditorController controller(nullptr, recentPath());
    controller.setText(QStringLiteral("scratch"));
    controller.discardDocument(0);

    QCOMPARE(controller.documentCount(), 1);
    QCOMPARE(controller.activeIndex(), 0);
    QVERIFY(controller.untitled());
    QVERIFY(controller.text().isEmpty());
    QVERIFY(!controller.anyDirty());
}

// --- Saving ----------------------------------------------------------------

void TextEditorControllerTest::savesNewDocumentsWithSaveAs()
{
    TextEditorController controller(nullptr, recentPath());
    controller.setText(QStringLiteral("A new Northstar document"));
    QVERIFY(controller.dirty());
    QVERIFY(!controller.save());
    QVERIFY(controller.statusIsError());

    const QString target = path(QStringLiteral("nested/new-document.txt"));
    QVERIFY(controller.saveAs(target));
    QCOMPARE(controller.filePath(), QFileInfo(target).absoluteFilePath());
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.untitled());

    QFile saved(target);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(saved.readAll(), QByteArray("A new Northstar document"));

    QCOMPARE(controller.suggestedSavePath(QStringLiteral("sibling.txt")),
             QFileInfo(target).absolutePath() + QStringLiteral("/sibling.txt"));
}

void TextEditorControllerTest::refusesSaveAsOverAnOpenDocument()
{
    const QString first = writeFile(QStringLiteral("first.txt"), "one");
    const QString second = writeFile(QStringLiteral("second.txt"), "two");

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.openFile(first));
    QVERIFY(controller.openFile(second));

    QVERIFY(!controller.saveAs(first));
    QVERIFY(controller.statusIsError());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("already open")));

    QFile untouched(first);
    QVERIFY(untouched.open(QIODevice::ReadOnly));
    QCOMPARE(untouched.readAll(), QByteArray("one"));

    QVERIFY(!controller.saveAs(m_directory->path()));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("folder")));
    QVERIFY(!controller.saveAs(QStringLiteral("relative.txt")));
    QVERIFY(!controller.saveAs(QString()));
}

void TextEditorControllerTest::reportsMissingUnreadableOversizedAndBinaryFiles()
{
    TextEditorController controller(nullptr, recentPath());

    QVERIFY(!controller.openFile(path(QStringLiteral("absent.txt"))));
    QVERIFY(controller.statusIsError());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("no longer available")));

    const QString binary = writeFile(QStringLiteral("binary.txt"), QByteArray("north\0star", 10));
    QVERIFY(!controller.openFile(binary));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("not a UTF-8 text file")));

    // A lone continuation byte is not valid UTF-8 either.
    const QString invalid = writeFile(QStringLiteral("invalid.txt"), QByteArray("valid \xB5 tail"));
    QVERIFY(!controller.openFile(invalid));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("not a UTF-8 text file")));

    const QString oversized = path(QStringLiteral("oversized.txt"));
    {
        QFile file(oversized);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(TextEditorController::maximumDocumentBytes() + 1));
    }
    QVERIFY(!controller.openFile(oversized));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("larger than")));

    const QString locked = writeFile(QStringLiteral("locked.txt"), "secret");
    QVERIFY(QFile::setPermissions(locked, QFile::Permissions()));
    if (QFileInfo(locked).isReadable()) {
        QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner);
        QSKIP("this account can read a mode 000 file, so the refusal cannot be observed");
    }
    QVERIFY(!controller.openFile(locked));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("permission")));
    QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner);

    // None of the refusals disturbed the document the user already had open.
    QCOMPARE(controller.documentCount(), 1);
    QVERIFY(controller.untitled());
}

void TextEditorControllerTest::recreatesADocumentRemovedOnDisk()
{
    const QString notes = writeFile(QStringLiteral("notes.txt"), "Northstar");

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.openFile(notes));
    controller.newDocument();
    QVERIFY(QFile::remove(notes));

    controller.activateDocument(0);
    QVERIFY(controller.missingOnDisk());
    QVERIFY(controller.statusIsError());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("removed on disk")));

    controller.setText(QStringLiteral("Northstar again"));
    QVERIFY(controller.save());
    QVERIFY(!controller.missingOnDisk());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("Recreated")));

    QFile restored(notes);
    QVERIFY(restored.open(QIODevice::ReadOnly));
    QCOMPARE(restored.readAll(), QByteArray("Northstar again"));
}

void TextEditorControllerTest::refusesToOverwriteExternalChangesUntilConfirmed()
{
    const QString notes = writeFile(QStringLiteral("notes.txt"), "Northstar");

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.openFile(notes));
    controller.setText(QStringLiteral("edited in the editor"));

    QVERIFY(!writeFile(QStringLiteral("notes.txt"),
                       "changed by another program entirely").isEmpty());
    QVERIFY(controller.externallyModified());

    QVERIFY(!controller.save());
    QVERIFY(controller.statusIsError());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("changed on disk")));
    QVERIFY(controller.dirty());

    QFile intact(notes);
    QVERIFY(intact.open(QIODevice::ReadOnly));
    QCOMPARE(intact.readAll(), QByteArray("changed by another program entirely"));
    intact.close();

    QVERIFY(controller.saveOverwritingExternalChanges());
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.externallyModified());

    QFile overwritten(notes);
    QVERIFY(overwritten.open(QIODevice::ReadOnly));
    QCOMPARE(overwritten.readAll(), QByteArray("edited in the editor"));
}

void TextEditorControllerTest::reloadsExternalChanges()
{
    const QString notes = writeFile(QStringLiteral("notes.txt"), "Northstar");

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.openFile(notes));
    controller.setText(QStringLiteral("local edit"));
    QVERIFY(!writeFile(QStringLiteral("notes.txt"), "external edit").isEmpty());

    QVERIFY(controller.reloadDocument());
    QCOMPARE(controller.text(), QStringLiteral("external edit"));
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.externallyModified());
}

void TextEditorControllerTest::preservesFilePermissionsOnSave()
{
    const QString notes = writeFile(QStringLiteral("notes.txt"), "Northstar");
    const QFile::Permissions permissions =
        QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup;
    QVERIFY(QFile::setPermissions(notes, permissions));

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.openFile(notes));
    controller.setText(QStringLiteral("Northstar edited"));
    QVERIFY(controller.save());

    QCOMPARE(QFile::permissions(notes) & (QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup),
             permissions);
}

// --- Find and replace ------------------------------------------------------

void TextEditorControllerTest::findsMatchesInBothDirections()
{
    TextEditorController controller(nullptr, recentPath());
    controller.setText(QStringLiteral("alpha beta alpha gamma alpha"));
    controller.setFindQuery(QStringLiteral("alpha"));
    QCOMPARE(controller.matchCount(), 3);
    QCOMPARE(controller.findSummary(), QStringLiteral("3 matches"));

    QSignalSpy selections(&controller, &TextEditorController::selectionRequested);

    controller.setCursorPosition(0);
    QVERIFY(controller.findNext());
    QCOMPARE(controller.currentMatch(), 1);
    QCOMPARE(selections.last().at(0).toInt(), 0);
    QCOMPARE(selections.last().at(1).toInt(), 5);

    QVERIFY(controller.findNext());
    QCOMPARE(controller.currentMatch(), 2);
    QCOMPARE(selections.last().at(0).toInt(), 11);
    QCOMPARE(selections.last().at(1).toInt(), 16);

    QVERIFY(controller.findNext());
    QCOMPARE(controller.currentMatch(), 3);
    QCOMPARE(controller.findSummary(), QStringLiteral("3 of 3"));

    // Searching past the last match wraps back to the first one.
    QVERIFY(controller.findNext());
    QCOMPARE(controller.currentMatch(), 1);

    // Find Previous steps upwards and wraps in the other direction.
    QVERIFY(controller.findPrevious());
    QCOMPARE(controller.currentMatch(), 3);
    QVERIFY(controller.findPrevious());
    QCOMPARE(controller.currentMatch(), 2);

    controller.setFindQuery(QStringLiteral("delta"));
    QCOMPARE(controller.matchCount(), 0);
    QCOMPARE(controller.findSummary(), QStringLiteral("No matches"));
    QVERIFY(!controller.findNext());
    QVERIFY(controller.statusIsError());
}

void TextEditorControllerTest::honoursCaseSensitivity()
{
    TextEditorController controller(nullptr, recentPath());
    controller.setText(QStringLiteral("Northstar northstar NORTHSTAR"));

    controller.setFindQuery(QStringLiteral("northstar"));
    QCOMPARE(controller.matchCount(), 3);

    controller.setFindCaseSensitive(true);
    QCOMPARE(controller.matchCount(), 1);

    controller.setFindCaseSensitive(false);
    QCOMPARE(controller.matchCount(), 3);
}

void TextEditorControllerTest::replacesTheSelectedMatchAndAllMatches()
{
    TextEditorController controller(nullptr, recentPath());
    controller.setText(QStringLiteral("alpha beta alpha"));
    controller.setFindQuery(QStringLiteral("alpha"));
    controller.setReplacementText(QStringLiteral("omega"));

    // The first press selects a match; the second replaces the selected one.
    controller.setCursorPosition(0);
    QVERIFY(controller.replaceCurrent());
    QCOMPARE(controller.text(), QStringLiteral("alpha beta alpha"));
    QVERIFY(controller.replaceCurrent());
    QCOMPARE(controller.text(), QStringLiteral("omega beta alpha"));
    QVERIFY(controller.dirty());

    QCOMPARE(controller.replaceAll(), 1);
    QCOMPARE(controller.text(), QStringLiteral("omega beta omega"));
    QCOMPARE(controller.matchCount(), 0);
    QVERIFY(controller.statusMessage().contains(QStringLiteral("Replaced one match")));

    controller.setFindQuery(QStringLiteral("omega"));
    controller.setReplacementText(QStringLiteral("x"));
    QCOMPARE(controller.replaceAll(), 2);
    QCOMPARE(controller.text(), QStringLiteral("x beta x"));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("Replaced 2 matches")));

    QCOMPARE(controller.replaceAll(), 0);
    QVERIFY(controller.statusIsError());
}

// --- Open browser ----------------------------------------------------------

void TextEditorControllerTest::browsesDirectoriesForOpening()
{
    QVERIFY(QDir().mkpath(path(QStringLiteral("folder"))));
    const QString notes = writeFile(QStringLiteral("notes.txt"), "Northstar");
    const QString oversized = path(QStringLiteral("oversized.txt"));
    {
        QFile file(oversized);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(TextEditorController::maximumDocumentBytes() + 1));
    }

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.browseTo(m_directory->path()));
    QVERIFY(!controller.browseTruncated());
    QVERIFY(controller.browseCanNavigateUp());

    bool sawFolder = false;
    bool sawNotes = false;
    bool sawOversized = false;
    for (const QVariant &value : controller.browseEntries()) {
        const QVariantMap entry = value.toMap();
        const QString name = entry.value(QStringLiteral("name")).toString();
        if (name == QStringLiteral("folder")) {
            sawFolder = true;
            QVERIFY(entry.value(QStringLiteral("isDirectory")).toBool());
            QVERIFY(entry.value(QStringLiteral("openable")).toBool());
        } else if (name == QStringLiteral("notes.txt")) {
            sawNotes = true;
            QVERIFY(!entry.value(QStringLiteral("isDirectory")).toBool());
            QVERIFY(entry.value(QStringLiteral("openable")).toBool());
        } else if (name == QStringLiteral("oversized.txt")) {
            sawOversized = true;
            QVERIFY(!entry.value(QStringLiteral("openable")).toBool());
            QVERIFY(entry.value(QStringLiteral("reason")).toString()
                        .contains(QStringLiteral("Larger than")));
        }
    }
    QVERIFY(sawFolder);
    QVERIFY(sawNotes);
    QVERIFY(sawOversized);

    QVERIFY(!controller.browseTo(notes));
    QVERIFY(controller.statusIsError());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("not a folder")));

    QVERIFY(controller.browseTo(path(QStringLiteral("folder"))));
    QCOMPARE(controller.browseEntries().size(), 0);
    QVERIFY(controller.browseUp());
    QCOMPARE(QFileInfo(controller.browsePath()).absoluteFilePath(),
             QFileInfo(m_directory->path()).absoluteFilePath());
}

// --- Recent files ----------------------------------------------------------

void TextEditorControllerTest::persistsRecentFilesAcrossSessions()
{
    const QString first = writeFile(QStringLiteral("first.txt"), "one");
    const QString second = writeFile(QStringLiteral("second.txt"), "two");

    {
        TextEditorController controller(nullptr, recentPath());
        QVERIFY(!controller.hasRecentFiles());
        QVERIFY(controller.openFile(first));
        QVERIFY(controller.openFile(second));
        QCOMPARE(controller.recentFiles().size(), 2);
    }

    TextEditorController restarted(nullptr, recentPath());
    QVERIFY(restarted.hasRecentFiles());
    QCOMPARE(restarted.recentFiles().size(), 2);

    // The most recently opened document is offered first.
    const QVariantMap newest = restarted.recentFiles().first().toMap();
    QCOMPARE(newest.value(QStringLiteral("path")).toString(),
             QFileInfo(second).absoluteFilePath());
    QCOMPARE(newest.value(QStringLiteral("name")).toString(), QStringLiteral("second.txt"));
    QVERIFY(newest.value(QStringLiteral("available")).toBool());

    QVERIFY(restarted.openRecent(1));
    QCOMPARE(restarted.filePath(), QFileInfo(first).absoluteFilePath());

    // The history is owner-private on disk.
    const QFile::Permissions permissions = QFile::permissions(recentPath());
    QVERIFY(!permissions.testFlag(QFile::ReadGroup));
    QVERIFY(!permissions.testFlag(QFile::ReadOther));
}

void TextEditorControllerTest::dropsRecentEntriesOnRequest()
{
    const QString first = writeFile(QStringLiteral("first.txt"), "one");
    const QString second = writeFile(QStringLiteral("second.txt"), "two");

    TextEditorController controller(nullptr, recentPath());
    QVERIFY(controller.openFile(first));
    QVERIFY(controller.openFile(second));
    QCOMPARE(controller.recentFiles().size(), 2);

    QVERIFY(controller.forgetRecent(0));
    QCOMPARE(controller.recentFiles().size(), 1);
    QCOMPARE(controller.recentFiles().first().toMap().value(QStringLiteral("path")).toString(),
             QFileInfo(first).absoluteFilePath());
    QVERIFY(!controller.forgetRecent(7));

    // Opening a history entry whose file has gone retires that entry. The tab
    // holding it is closed first so this exercises a real reopen.
    QVERIFY(QFile::remove(first));
    controller.discardDocument(0);
    QVERIFY(!controller.openRecent(0));
    QVERIFY(controller.statusIsError());
    QCOMPARE(controller.recentFiles().size(), 0);

    const QString third = writeFile(QStringLiteral("third.txt"), "three");
    QVERIFY(controller.openFile(third));
    QCOMPARE(controller.recentFiles().size(), 1);
    controller.clearRecentFiles();
    QVERIFY(!controller.hasRecentFiles());

    TextEditorController restarted(nullptr, recentPath());
    QVERIFY(!restarted.hasRecentFiles());
    QCOMPARE(RecentFilesStore(recentPath()).paths().size(), 0);
}

QTEST_MAIN(TextEditorControllerTest)
#include "test-texteditorcontroller.moc"
