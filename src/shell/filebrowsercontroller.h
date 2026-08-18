#pragma once

#include <functional>

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

class FileBrowserController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString displayPath READ displayPath NOTIFY locationChanged)
    Q_PROPERTY(QString locationRoot READ locationRoot NOTIFY locationChanged)
    Q_PROPERTY(QString homePath READ homePath CONSTANT)
    Q_PROPERTY(bool homeLocation READ homeLocation NOTIFY locationChanged)
    Q_PROPERTY(bool readOnlyLocation READ readOnlyLocation NOTIFY locationChanged)
    Q_PROPERTY(bool canNavigateUp READ canNavigateUp NOTIFY locationChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchQueryChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool showingTrash READ showingTrash NOTIFY locationChanged)
    Q_PROPERTY(QString sortMode READ sortMode NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)
    Q_PROPERTY(QVariantList desktopEntries READ desktopEntries NOTIFY desktopEntriesChanged)
    Q_PROPERTY(QString clipboardOperation READ clipboardOperation NOTIFY clipboardChanged)
    Q_PROPERTY(QString clipboardName READ clipboardName NOTIFY clipboardChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    Q_PROPERTY(bool conflictPending READ conflictPending NOTIFY conflictChanged)
    Q_PROPERTY(QString conflictName READ conflictName NOTIFY conflictChanged)
    Q_PROPERTY(QString transferStatus READ transferStatus NOTIFY transferChanged)
    Q_PROPERTY(int transferProgress READ transferProgress NOTIFY transferChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)
    Q_PROPERTY(QString undoLabel READ undoLabel NOTIFY undoChanged)

public:
    using OpenFunction = std::function<bool(const QUrl &url)>;

    explicit FileBrowserController(QObject *parent = nullptr,
                                   QString rootPath = {},
                                   OpenFunction openFunction = {},
                                   QStringList mountedLocationRoots = {});

    QVariantList entries() const;
    QString currentPath() const;
    QString displayPath() const;
    QString locationRoot() const;
    QString homePath() const;
    bool homeLocation() const;
    bool readOnlyLocation() const;
    bool canNavigateUp() const;
    QString searchQuery() const;
    bool searching() const;
    QString errorMessage() const;
    bool showingTrash() const;
    QString sortMode() const;
    bool sortAscending() const;
    QVariantList desktopEntries() const;
    QString clipboardOperation() const;
    QString clipboardName() const;
    bool canPaste() const;
    bool conflictPending() const;
    QString conflictName() const;
    QString transferStatus() const;
    int transferProgress() const;
    bool canUndo() const;
    QString undoLabel() const;

    Q_INVOKABLE bool navigateTo(const QString &path);
    Q_INVOKABLE bool openLocation(const QString &path, const QString &label = {});
    Q_INVOKABLE QString homeChildPath(const QString &relativePath) const;
    Q_INVOKABLE bool navigateUp();
    Q_INVOKABLE bool goHome();
    Q_INVOKABLE bool showTrash();
    Q_INVOKABLE bool openEntry(const QString &path);
    Q_INVOKABLE bool createFolder(const QString &name);
    Q_INVOKABLE bool createFile(const QString &name);
    Q_INVOKABLE bool renameEntry(const QString &path, const QString &newName);
    Q_INVOKABLE bool moveToTrash(const QString &path);
    Q_INVOKABLE bool restoreEntry(const QString &path);
    Q_INVOKABLE bool emptyTrash();
    Q_INVOKABLE bool copyEntry(const QString &path);
    Q_INVOKABLE bool cutEntry(const QString &path);
    Q_INVOKABLE bool pasteClipboard();
    Q_INVOKABLE bool pasteClipboard(const QString &conflictResolution);
    Q_INVOKABLE void cancelConflict();
    Q_INVOKABLE void clearClipboard();
    Q_INVOKABLE bool undoLastTransfer();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setSortMode(const QString &mode);
    Q_INVOKABLE void toggleSortOrder();

public slots:
    void setSearchQuery(const QString &query);

signals:
    void entriesChanged();
    void currentPathChanged();
    void locationChanged();
    void searchQueryChanged();
    void errorMessageChanged();
    void sortChanged();
    void desktopEntriesChanged();
    void clipboardChanged();
    void conflictChanged();
    void transferChanged();
    void undoChanged();

private:
    static QString normalizedPath(const QString &path);
    static QString canonicalOrNormalizedPath(const QString &path);
    static bool isValidEntryName(const QString &name);
    QString resolvePath(const QString &path) const;
    QString resolveTrashPath(const QString &path) const;
    bool isWithinRoot(const QString &path) const;
    bool isWithinNavigationRoot(const QString &path) const;
    bool isWithinTrash(const QString &path) const;
    bool isMountedLocationRoot(const QString &path) const;
    QString trashFilesPath() const;
    QString trashInfoPath() const;
    QString uniqueTrashName(const QString &name) const;
    bool ensureTrashDirectories() const;
    bool readTrashOriginalPath(const QString &trashPath, QString *originalPath) const;
    bool writeTrashInfo(const QString &infoPath, const QString &originalPath) const;
    bool isAllowedClipboardSource(const QString &path) const;
    QString keepBothPath(const QString &destinationPath) const;
    void clearConflict();
    void clearUndo();
    void setTransferStatus(const QString &status, int progress);
    void clearSearchQuery();
    void refreshSearchResults();
    void sortEntries(QVariantList *entries) const;
    void refreshDesktopEntries();
    void setErrorMessage(const QString &message);

    QString m_rootPath;
    QString m_navigationRoot;
    QString m_locationLabel;
    QStringList m_mountedLocationRoots;
    QString m_currentPath;
    QString m_searchQuery;
    QVariantList m_entries;
    QVariantList m_desktopEntries;
    OpenFunction m_openFunction;
    QString m_errorMessage;
    QString m_sortMode = QStringLiteral("name");
    bool m_sortAscending = true;
    bool m_showingTrash = false;
    QString m_clipboardPath;
    QString m_clipboardOperation;
    QString m_conflictDestination;
    QString m_transferStatus;
    QString m_undoSource;
    QString m_undoDestination;
    QString m_undoOperation;
    int m_transferProgress = 0;
    QFileSystemWatcher *m_desktopWatcher = nullptr;
    QTimer *m_desktopRefreshTimer = nullptr;
};
