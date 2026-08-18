#pragma once

#include "recentfilesstore.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

// Multiple-document text editing for the Northstar Text Editor.
//
// The controller owns every open document, the user-private recent-file
// history, the find/replace state, and the in-application open browser. QML
// renders the tab strip and dialogs; it never touches the filesystem itself.
class TextEditorController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList documents READ documents NOTIFY documentsChanged)
    Q_PROPERTY(int documentCount READ documentCount NOTIFY documentsChanged)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY documentsChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY stateChanged)
    Q_PROPERTY(QString displayPath READ displayPath NOTIFY stateChanged)
    Q_PROPERTY(QString documentTitle READ documentTitle NOTIFY stateChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY stateChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY stateChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY stateChanged)
    Q_PROPERTY(bool untitled READ untitled NOTIFY stateChanged)
    Q_PROPERTY(bool anyDirty READ anyDirty NOTIFY documentsChanged)
    Q_PROPERTY(int firstDirtyIndex READ firstDirtyIndex NOTIFY documentsChanged)
    Q_PROPERTY(bool missingOnDisk READ missingOnDisk NOTIFY stateChanged)
    Q_PROPERTY(bool externallyModified READ externallyModified NOTIFY stateChanged)
    Q_PROPERTY(QString defaultSaveDirectory READ defaultSaveDirectory CONSTANT)
    Q_PROPERTY(qint64 maximumDocumentBytes READ maximumDocumentBytes CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY stateChanged)

    Q_PROPERTY(QVariantList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    Q_PROPERTY(bool hasRecentFiles READ hasRecentFiles NOTIFY recentFilesChanged)

    Q_PROPERTY(QString findQuery READ findQuery WRITE setFindQuery NOTIFY findChanged)
    Q_PROPERTY(QString replacementText READ replacementText WRITE setReplacementText NOTIFY findChanged)
    Q_PROPERTY(bool findCaseSensitive READ findCaseSensitive WRITE setFindCaseSensitive NOTIFY findChanged)
    Q_PROPERTY(int matchCount READ matchCount NOTIFY findChanged)
    Q_PROPERTY(int currentMatch READ currentMatch NOTIFY findChanged)
    Q_PROPERTY(QString findSummary READ findSummary NOTIFY findChanged)

    Q_PROPERTY(QString browsePath READ browsePath NOTIFY browseChanged)
    Q_PROPERTY(QString browseDisplayPath READ browseDisplayPath NOTIFY browseChanged)
    Q_PROPERTY(QVariantList browseEntries READ browseEntries NOTIFY browseChanged)
    Q_PROPERTY(bool browseCanNavigateUp READ browseCanNavigateUp NOTIFY browseChanged)
    Q_PROPERTY(bool browseTruncated READ browseTruncated NOTIFY browseChanged)

public:
    explicit TextEditorController(QObject *parent = nullptr, QString recentFilesPath = {});

    static qint64 maximumDocumentBytes();

    QVariantList documents() const;
    int documentCount() const;
    int activeIndex() const;
    QString filePath() const;
    QString displayPath() const;
    QString documentTitle() const;
    QString text() const;
    bool dirty() const;
    bool canSave() const;
    bool untitled() const;
    bool anyDirty() const;
    int firstDirtyIndex() const;
    bool missingOnDisk() const;
    bool externallyModified() const;
    QString defaultSaveDirectory() const;
    QString statusMessage() const;
    bool statusIsError() const;

    QVariantList recentFiles() const;
    bool hasRecentFiles() const;

    QString findQuery() const;
    QString replacementText() const;
    bool findCaseSensitive() const;
    int matchCount() const;
    int currentMatch() const;
    QString findSummary() const;

    QString browsePath() const;
    QString browseDisplayPath() const;
    QVariantList browseEntries() const;
    bool browseCanNavigateUp() const;
    bool browseTruncated() const;

    // Documents and tabs.
    Q_INVOKABLE int newDocument();
    Q_INVOKABLE bool openFile(const QString &path);
    Q_INVOKABLE void activateDocument(int index);
    Q_INVOKABLE bool closeDocument(int index);
    Q_INVOKABLE void discardDocument(int index);
    Q_INVOKABLE bool reloadDocument();

    // Saving.
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveOverwritingExternalChanges();
    Q_INVOKABLE bool saveAs(const QString &path);
    Q_INVOKABLE QString suggestedSavePath(const QString &fileName) const;

    // Recent files.
    Q_INVOKABLE bool openRecent(int index);
    Q_INVOKABLE bool forgetRecent(int index);
    Q_INVOKABLE void clearRecentFiles();

    // Find and replace.
    Q_INVOKABLE bool findNext();
    Q_INVOKABLE bool findPrevious();
    Q_INVOKABLE bool replaceCurrent();
    Q_INVOKABLE int replaceAll();
    Q_INVOKABLE void setCursorPosition(int position);

    // In-application open browser.
    Q_INVOKABLE bool browseTo(const QString &path);
    Q_INVOKABLE bool browseUp();
    Q_INVOKABLE bool browseHome();
    Q_INVOKABLE void refreshBrowse();

public slots:
    void setText(const QString &text);
    void setFindQuery(const QString &query);
    void setReplacementText(const QString &replacement);
    void setFindCaseSensitive(bool caseSensitive);

signals:
    void stateChanged();
    void documentsChanged();
    void recentFilesChanged();
    void findChanged();
    void browseChanged();
    void selectionRequested(int anchor, int cursor);

private:
    struct Document
    {
        int id = 0;
        QString filePath;
        QString untitledName;
        QString text;
        QString savedText;
        qint64 savedSize = -1;
        QDateTime savedModified;
        bool missing = false;
    };

    enum class LoadOutcome
    {
        Loaded,
        Missing,
        Unreadable,
        TooLarge,
        NotText,
    };

    static QString normalizedPath(const QString &path);
    static QString displayNameFor(const QString &path);
    static QString abbreviatedPath(const QString &path);
    LoadOutcome readDocument(const QString &path, QString *contents) const;
    void reportLoadFailure(LoadOutcome outcome, const QString &path);

    Document *activeDocument();
    const Document *activeDocument() const;
    int indexOfPath(const QString &path) const;
    QString nextUntitledName();
    void appendDocument(Document document, bool activate);
    bool replaceEmptyUntitledDocument() const;

    bool writeDocument(const Document &document, const QString &path) const;
    void recordSavedFileState(Document *document, const QString &path);
    bool commitSave(Document *document, const QString &path, bool allowExternalOverwrite);

    void refreshRecentFiles();
    void recomputeMatches();
    void announce(const QString &message, bool error = false);
    void emitDocumentState();

    QList<Document> m_documents;
    int m_activeIndex = -1;
    int m_nextDocumentId = 1;
    int m_untitledCounter = 0;

    QString m_statusMessage;
    bool m_statusIsError = false;

    RecentFilesStore m_recentFiles;
    QVariantList m_recentFileEntries;

    QString m_findQuery;
    QString m_replacementText;
    bool m_findCaseSensitive = false;
    QList<int> m_matchPositions;
    int m_currentMatch = -1;
    int m_cursorPosition = 0;
    int m_expectedCursor = -1;

    QString m_browsePath;
    QVariantList m_browseEntries;
    bool m_browseTruncated = false;
};
