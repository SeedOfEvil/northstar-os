#include "filebrowsercontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QDesktopServices>
#include <QDateTime>
#include <QDirIterator>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QtConcurrentRun>

#include <algorithm>

#include <utility>

namespace {

constexpr qsizetype MaximumSearchResults = 500;

bool pathMatchesRoot(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root + QLatin1Char('/'));
}

bool isLaunchableDesktopEntry(const QFileInfo &info)
{
    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("desktop") || suffix == QStringLiteral("app");
}

bool removeEntryRecursively(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        return true;
    }
    if (info.isDir() && !info.isSymLink()) {
        return QDir(path).removeRecursively();
    }
    return QFile::remove(path);
}

bool copyEntryRecursively(const QString &sourcePath, const QString &destinationPath)
{
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || sourceInfo.isSymLink()) {
        return false;
    }
    if (!sourceInfo.isDir()) {
        return QFile::copy(sourcePath, destinationPath);
    }

    if (!QDir().mkpath(destinationPath)) {
        return false;
    }
    const QFileInfoList children = QDir(sourcePath).entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &child : children) {
        if (child.isSymLink()
            || !copyEntryRecursively(child.absoluteFilePath(),
                                     QDir(destinationPath).filePath(child.fileName()))) {
            removeEntryRecursively(destinationPath);
            return false;
        }
    }
    return true;
}

} // namespace

FileBrowserController::FileBrowserController(QObject *parent,
                                             QString rootPath,
                                             OpenFunction openFunction,
                                             QStringList mountedLocationRoots)
    : QObject(parent)
    , m_rootPath(canonicalOrNormalizedPath(rootPath.isEmpty() ? QDir::homePath() : rootPath))
    , m_navigationRoot(m_rootPath)
    , m_locationLabel(QStringLiteral("Home"))
    , m_mountedLocationRoots(std::move(mountedLocationRoots))
    , m_currentPath(m_rootPath)
    , m_openFunction(std::move(openFunction))
{
    m_desktopWatcher = new QFileSystemWatcher(this);
    m_desktopRefreshTimer = new QTimer(this);
    m_desktopRefreshTimer->setInterval(100);
    m_desktopRefreshTimer->setSingleShot(true);
    connect(m_desktopWatcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString &) {
        if (m_desktopRefreshTimer != nullptr) {
            m_desktopRefreshTimer->start();
        }
    });
    connect(m_desktopRefreshTimer, &QTimer::timeout,
            this, &FileBrowserController::refreshDesktopEntries);
    refresh();
}

QVariantList FileBrowserController::entries() const
{
    return m_entries;
}

QString FileBrowserController::currentPath() const
{
    return m_currentPath;
}

QString FileBrowserController::displayPath() const
{
    if (m_showingTrash) {
        return QStringLiteral("Trash");
    }

    if (!m_searchQuery.isEmpty()) {
        return QStringLiteral("Search: %1").arg(m_searchQuery);
    }

    if (m_currentPath == m_navigationRoot && !m_locationLabel.isEmpty()) {
        if (!homeLocation()) {
            return QStringLiteral("%1 (%2)").arg(m_locationLabel, m_navigationRoot);
        }
        return QStringLiteral("~");
    }

    if (homeLocation() && m_currentPath == m_rootPath) {
        return QStringLiteral("~");
    }

    const QString prefix = m_rootPath + QLatin1Char('/');
    if (homeLocation() && m_currentPath.startsWith(prefix)) {
        return QStringLiteral("~/") + m_currentPath.mid(prefix.size());
    }
    return m_currentPath;
}

QString FileBrowserController::locationRoot() const
{
    return m_navigationRoot;
}

QString FileBrowserController::homePath() const
{
    return m_rootPath;
}

bool FileBrowserController::homeLocation() const
{
    return !m_showingTrash && m_navigationRoot == m_rootPath;
}

bool FileBrowserController::readOnlyLocation() const
{
    return !m_showingTrash && !homeLocation();
}

bool FileBrowserController::canNavigateUp() const
{
    return !m_showingTrash && m_currentPath != m_navigationRoot;
}

QString FileBrowserController::searchQuery() const
{
    return m_searchQuery;
}

bool FileBrowserController::searching() const
{
    return !m_searchQuery.isEmpty() && homeLocation();
}

QString FileBrowserController::errorMessage() const
{
    return m_errorMessage;
}

bool FileBrowserController::showingTrash() const
{
    return m_showingTrash;
}

QString FileBrowserController::sortMode() const
{
    return m_sortMode;
}

bool FileBrowserController::sortAscending() const
{
    return m_sortAscending;
}

QVariantList FileBrowserController::desktopEntries() const
{
    return m_desktopEntries;
}

QString FileBrowserController::clipboardOperation() const
{
    return m_clipboardOperation;
}

QString FileBrowserController::clipboardName() const
{
    return QFileInfo(m_clipboardPath).fileName();
}

bool FileBrowserController::canPaste() const
{
    return !m_showingTrash && homeLocation() && !m_clipboardPath.isEmpty()
        && QFileInfo::exists(m_clipboardPath) && !m_transferActive;
}

bool FileBrowserController::conflictPending() const
{
    return !m_conflictDestination.isEmpty();
}

QString FileBrowserController::conflictName() const
{
    return QFileInfo(m_conflictDestination).fileName();
}

QString FileBrowserController::transferStatus() const
{
    return m_transferStatus;
}

int FileBrowserController::transferProgress() const
{
    return m_transferProgress;
}

bool FileBrowserController::transferActive() const
{
    return m_transferActive;
}

bool FileBrowserController::canUndo() const
{
    return !m_undoOperation.isEmpty();
}

QString FileBrowserController::undoLabel() const
{
    if (m_undoOperation == QStringLiteral("copy")) {
        return QStringLiteral("Undo copy");
    }
    if (m_undoOperation == QStringLiteral("cut")) {
        return QStringLiteral("Undo move");
    }
    return {};
}

bool FileBrowserController::navigateTo(const QString &path)
{
    const bool wasShowingTrash = m_showingTrash;
    const QString resolvedPath = resolvePath(path);
    if (resolvedPath.isEmpty() || !isWithinNavigationRoot(resolvedPath)) {
        setErrorMessage(readOnlyLocation()
                ? QStringLiteral("That location is outside the mounted volume.")
                : QStringLiteral("That location is outside the Northstar home folder."));
        return false;
    }

    const QFileInfo info(resolvedPath);
    if (!info.exists() || !info.isDir()) {
        setErrorMessage(QStringLiteral("That folder is not available."));
        return false;
    }

    clearSearchQuery();

    if (m_currentPath != resolvedPath) {
        m_currentPath = resolvedPath;
        emit currentPathChanged();
        emit locationChanged();
    }
    m_showingTrash = false;
    if (wasShowingTrash) {
        emit locationChanged();
    }
    setErrorMessage({});
    refresh();
    emit clipboardChanged();
    return true;
}

bool FileBrowserController::openLocation(const QString &path, const QString &label)
{
    const QString resolvedPath = canonicalOrNormalizedPath(path);
    if (resolvedPath.isEmpty()) {
        setErrorMessage(QStringLiteral("That volume is not available."));
        return false;
    }
    if (resolvedPath == m_rootPath) {
        return goHome();
    }
    if (!isMountedLocationRoot(resolvedPath)) {
        setErrorMessage(QStringLiteral("That location is not a mounted volume."));
        return false;
    }

    const QFileInfo info(resolvedPath);
    if (!info.isDir()) {
        setErrorMessage(QStringLiteral("That volume is not available."));
        return false;
    }

    clearSearchQuery();
    const bool pathChanged = m_currentPath != resolvedPath;
    m_navigationRoot = resolvedPath;
    m_locationLabel = label.trimmed().isEmpty() ? resolvedPath : label.trimmed();
    m_showingTrash = false;
    m_currentPath = resolvedPath;
    if (pathChanged) {
        emit currentPathChanged();
    }
    emit locationChanged();
    setErrorMessage({});
    refresh();
    emit clipboardChanged();
    return true;
}

QString FileBrowserController::homeChildPath(const QString &relativePath) const
{
    const QString trimmedPath = relativePath.trimmed();
    if (trimmedPath.isEmpty() || QFileInfo(trimmedPath).isAbsolute()) {
        return {};
    }

    const QString candidatePath = canonicalOrNormalizedPath(QDir(m_rootPath).filePath(trimmedPath));
    if (candidatePath.isEmpty() || !isWithinRoot(candidatePath)) {
        return {};
    }

    const QFileInfo candidateInfo(candidatePath);
    return candidateInfo.exists() && candidateInfo.isDir() ? candidatePath : QString();
}

bool FileBrowserController::navigateUp()
{
    if (!canNavigateUp()) {
        return false;
    }

    const QFileInfo currentInfo(m_currentPath);
    return navigateTo(currentInfo.dir().absolutePath());
}

bool FileBrowserController::goHome()
{
    clearSearchQuery();
    const bool wasShowingTrash = m_showingTrash;
    const bool locationRootChanged = m_navigationRoot != m_rootPath
        || m_locationLabel != QStringLiteral("Home");
    const bool pathChanged = m_currentPath != m_rootPath;
    m_navigationRoot = m_rootPath;
    m_locationLabel = QStringLiteral("Home");
    m_showingTrash = false;
    m_currentPath = m_rootPath;
    if (pathChanged) {
        emit currentPathChanged();
    }
    if (wasShowingTrash || locationRootChanged || pathChanged) {
        emit locationChanged();
    }
    setErrorMessage({});
    refresh();
    emit clipboardChanged();
    return true;
}

bool FileBrowserController::showTrash()
{
    clearSearchQuery();
    if (!ensureTrashDirectories()) {
        setErrorMessage(QStringLiteral("Unable to prepare the Northstar Trash."));
        return false;
    }

    const bool changed = !m_showingTrash;
    m_showingTrash = true;
    if (changed) {
        emit locationChanged();
    }
    setErrorMessage({});
    refresh();
    emit clipboardChanged();
    return true;
}

void FileBrowserController::setSearchQuery(const QString &query)
{
    const QString normalizedQuery = query.simplified();
    if ((!homeLocation() || m_showingTrash) && !normalizedQuery.isEmpty()) {
        setErrorMessage(QStringLiteral("Search is available from the Northstar home folder."));
        return;
    }
    if (m_searchQuery == normalizedQuery) {
        return;
    }

    m_searchQuery = normalizedQuery;
    emit searchQueryChanged();
    setErrorMessage({});
    refresh();
}

bool FileBrowserController::openEntry(const QString &path)
{
    if (m_showingTrash) {
        const QString resolvedTrashPath = resolveTrashPath(path);
        if (resolvedTrashPath.isEmpty() || !isWithinTrash(resolvedTrashPath)) {
            setErrorMessage(QStringLiteral("That item is not in the Northstar Trash."));
            return false;
        }

        const QFileInfo trashInfo(resolvedTrashPath);
        if (!trashInfo.exists()) {
            setErrorMessage(QStringLiteral("That item is no longer in the Northstar Trash."));
            return false;
        }
        if (trashInfo.isDir()) {
            setErrorMessage(QStringLiteral("Restore a folder before opening it."));
            return false;
        }

        const bool opened = m_openFunction
            ? m_openFunction(QUrl::fromLocalFile(resolvedTrashPath))
            : QDesktopServices::openUrl(QUrl::fromLocalFile(resolvedTrashPath));
        if (!opened) {
            setErrorMessage(QStringLiteral("No application could open %1.").arg(trashInfo.fileName()));
            return false;
        }

        setErrorMessage({});
        return true;
    }

    const QString resolvedPath = resolvePath(path);
    if (resolvedPath.isEmpty() || !isWithinNavigationRoot(resolvedPath)) {
        setErrorMessage(readOnlyLocation()
                ? QStringLiteral("That item is outside the mounted volume.")
                : QStringLiteral("That item is outside the Northstar home folder."));
        return false;
    }

    const QFileInfo info(resolvedPath);
    if (!info.exists()) {
        setErrorMessage(QStringLiteral("That item is no longer available."));
        return false;
    }
    if (info.isDir()) {
        return navigateTo(resolvedPath);
    }

    const QUrl url = QUrl::fromLocalFile(resolvedPath);
    const bool opened = m_openFunction ? m_openFunction(url) : QDesktopServices::openUrl(url);
    if (!opened) {
        setErrorMessage(QStringLiteral("No application could open %1.").arg(info.fileName()));
        return false;
    }

    setErrorMessage({});
    return true;
}

bool FileBrowserController::createFolder(const QString &name)
{
    if (m_showingTrash) {
        setErrorMessage(QStringLiteral("Go Home before creating a folder."));
        return false;
    }
    if (!homeLocation()) {
        setErrorMessage(QStringLiteral("Return to Home before changing files."));
        return false;
    }

    const QString trimmedName = name.trimmed();
    if (!isValidEntryName(trimmedName)) {
        setErrorMessage(QStringLiteral("Choose a folder name without path separators."));
        return false;
    }

    const QString folderPath = normalizedPath(QDir(m_currentPath).filePath(trimmedName));
    if (!isWithinRoot(folderPath)) {
        setErrorMessage(QStringLiteral("That folder would leave the Northstar home folder."));
        return false;
    }
    if (QFileInfo::exists(folderPath)) {
        setErrorMessage(QStringLiteral("An item with that name already exists."));
        return false;
    }
    if (!QDir(m_currentPath).mkdir(trimmedName)) {
        setErrorMessage(QStringLiteral("Unable to create the folder."));
        return false;
    }

    setErrorMessage({});
    refresh();
    return true;
}

bool FileBrowserController::createFile(const QString &name)
{
    if (m_showingTrash) {
        setErrorMessage(QStringLiteral("Go Home before creating a file."));
        return false;
    }
    if (!homeLocation()) {
        setErrorMessage(QStringLiteral("Return to Home before changing files."));
        return false;
    }

    const QString trimmedName = name.trimmed();
    if (!isValidEntryName(trimmedName)) {
        setErrorMessage(QStringLiteral("Choose a file name without path separators."));
        return false;
    }

    const QString filePath = normalizedPath(QDir(m_currentPath).filePath(trimmedName));
    if (!isWithinRoot(filePath)) {
        setErrorMessage(QStringLiteral("That file would leave the Northstar home folder."));
        return false;
    }
    const QFileInfo destinationInfo(filePath);
    if (destinationInfo.exists() || destinationInfo.isSymLink()) {
        setErrorMessage(QStringLiteral("An item with that name already exists."));
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setErrorMessage(QStringLiteral("Unable to create the file."));
        return false;
    }
    file.close();

    setErrorMessage({});
    refresh();
    return true;
}

bool FileBrowserController::renameEntry(const QString &path, const QString &newName)
{
    if (m_showingTrash) {
        setErrorMessage(QStringLiteral("Restore an item before renaming it."));
        return false;
    }
    if (!homeLocation()) {
        setErrorMessage(QStringLiteral("Return to Home before changing files."));
        return false;
    }

    const QString resolvedPath = resolvePath(path);
    const QString trimmedName = newName.trimmed();
    if (resolvedPath.isEmpty() || !isWithinRoot(resolvedPath)) {
        setErrorMessage(QStringLiteral("That item is outside the Northstar home folder."));
        return false;
    }
    if (resolvedPath == m_rootPath) {
        setErrorMessage(QStringLiteral("The Northstar home folder cannot be renamed."));
        return false;
    }
    if (!isValidEntryName(trimmedName)) {
        setErrorMessage(QStringLiteral("Choose a name without path separators."));
        return false;
    }

    const QFileInfo sourceInfo(resolvedPath);
    if (!sourceInfo.exists()) {
        setErrorMessage(QStringLiteral("That item is no longer available."));
        return false;
    }

    const QString destinationPath = normalizedPath(sourceInfo.dir().filePath(trimmedName));
    if (!isWithinRoot(destinationPath)) {
        setErrorMessage(QStringLiteral("That name would leave the Northstar home folder."));
        return false;
    }
    if (destinationPath == resolvedPath) {
        setErrorMessage({});
        return true;
    }
    if (QFileInfo::exists(destinationPath)) {
        setErrorMessage(QStringLiteral("An item with that name already exists."));
        return false;
    }
    if (!QFile::rename(resolvedPath, destinationPath)) {
        setErrorMessage(QStringLiteral("Unable to rename that item."));
        return false;
    }

    setErrorMessage({});
    refresh();
    return true;
}

bool FileBrowserController::moveToTrash(const QString &path)
{
    if (m_showingTrash) {
        setErrorMessage(QStringLiteral("An item cannot be moved to Trash from the Trash view."));
        return false;
    }
    if (!homeLocation()) {
        setErrorMessage(QStringLiteral("Return to Home before changing files."));
        return false;
    }

    const QString resolvedPath = resolvePath(path);
    if (resolvedPath.isEmpty() || !isWithinRoot(resolvedPath)) {
        setErrorMessage(QStringLiteral("That item is outside the Northstar home folder."));
        return false;
    }
    if (resolvedPath == m_rootPath) {
        setErrorMessage(QStringLiteral("The Northstar home folder cannot be moved to Trash."));
        return false;
    }
    if (pathMatchesRoot(normalizedPath(resolvedPath), normalizedPath(trashFilesPath()))) {
        setErrorMessage(QStringLiteral("That item is already in Trash."));
        return false;
    }

    const QFileInfo sourceInfo(resolvedPath);
    if (!sourceInfo.exists()) {
        setErrorMessage(QStringLiteral("That item is no longer available."));
        return false;
    }
    if (!ensureTrashDirectories()) {
        setErrorMessage(QStringLiteral("Unable to prepare the Northstar Trash."));
        return false;
    }

    const QString trashName = uniqueTrashName(sourceInfo.fileName());
    if (trashName.isEmpty()) {
        setErrorMessage(QStringLiteral("Unable to choose a safe Trash name."));
        return false;
    }
    const QString destinationPath = QDir(trashFilesPath()).filePath(trashName);
    const QString infoPath = QDir(trashInfoPath()).filePath(trashName + QStringLiteral(".trashinfo"));
    if (!QFile::rename(resolvedPath, destinationPath)) {
        setErrorMessage(QStringLiteral("Unable to move that item to Trash."));
        return false;
    }
    if (!writeTrashInfo(infoPath, resolvedPath)) {
        QFile::rename(destinationPath, resolvedPath);
        setErrorMessage(QStringLiteral("Unable to record the Trash metadata."));
        return false;
    }

    setErrorMessage({});
    refresh();
    return true;
}

bool FileBrowserController::restoreEntry(const QString &path)
{
    if (!m_showingTrash) {
        setErrorMessage(QStringLiteral("Open Trash before restoring an item."));
        return false;
    }

    const QString resolvedTrashPath = resolveTrashPath(path);
    if (resolvedTrashPath.isEmpty() || !isWithinTrash(resolvedTrashPath)) {
        setErrorMessage(QStringLiteral("That item is not in the Northstar Trash."));
        return false;
    }

    const QFileInfo trashInfo(resolvedTrashPath);
    if (!trashInfo.exists()) {
        setErrorMessage(QStringLiteral("That item is no longer in the Northstar Trash."));
        return false;
    }

    QString originalPath;
    if (!readTrashOriginalPath(resolvedTrashPath, &originalPath)) {
        setErrorMessage(QStringLiteral("That Trash entry has no safe restore location."));
        return false;
    }
    if (QFileInfo::exists(originalPath)) {
        setErrorMessage(QStringLiteral("The original location already contains an item with that name."));
        return false;
    }

    const QFileInfo originalInfo(originalPath);
    if (!originalInfo.dir().exists()) {
        setErrorMessage(QStringLiteral("The original folder is no longer available."));
        return false;
    }
    if (!QFile::rename(resolvedTrashPath, originalPath)) {
        setErrorMessage(QStringLiteral("Unable to restore that item."));
        return false;
    }

    const QString infoPath = QDir(trashInfoPath()).filePath(trashInfo.fileName() + QStringLiteral(".trashinfo"));
    if (!QFile::remove(infoPath)) {
        setErrorMessage(QStringLiteral("Item restored, but Trash metadata could not be removed."));
    } else {
        setErrorMessage({});
    }
    refresh();
    return true;
}

bool FileBrowserController::emptyTrash()
{
    if (!ensureTrashDirectories()) {
        setErrorMessage(QStringLiteral("Unable to prepare the Northstar Trash."));
        return false;
    }

    bool removedAll = true;
    const QDir filesDirectory(trashFilesPath());
    const QFileInfoList trashedEntries = filesDirectory.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const QFileInfo &info : trashedEntries) {
        const bool removed = info.isDir() && !info.isSymLink()
            ? QDir(info.absoluteFilePath()).removeRecursively()
            : QFile::remove(info.absoluteFilePath());
        removedAll = removed && removedAll;
    }

    const QDir infoDirectory(trashInfoPath());
    const QFileInfoList metadataEntries = infoDirectory.entryInfoList(
        QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const QFileInfo &info : metadataEntries) {
        removedAll = QFile::remove(info.absoluteFilePath()) && removedAll;
    }

    if (!removedAll) {
        setErrorMessage(QStringLiteral("Some Trash items could not be removed."));
        return false;
    }

    setErrorMessage({});
    if (m_showingTrash) {
        refresh();
    }
    return true;
}

bool FileBrowserController::copyEntry(const QString &path)
{
    if (m_showingTrash) {
        setErrorMessage(QStringLiteral("Restore an item before copying it."));
        return false;
    }
    const QString resolvedPath = resolvePath(path);
    const QFileInfo sourceInfo(resolvedPath);
    if (resolvedPath.isEmpty() || !isAllowedClipboardSource(resolvedPath)
        || !sourceInfo.exists() || sourceInfo.isSymLink()) {
        setErrorMessage(QStringLiteral("That item cannot be copied safely."));
        return false;
    }

    m_clipboardPath = resolvedPath;
    m_clipboardOperation = QStringLiteral("copy");
    clearConflict();
    setTransferStatus(QStringLiteral("Ready to copy %1.").arg(sourceInfo.fileName()), 0);
    setErrorMessage({});
    emit clipboardChanged();
    return true;
}

bool FileBrowserController::cutEntry(const QString &path)
{
    if (m_showingTrash || !homeLocation()) {
        setErrorMessage(QStringLiteral("Only items in Home can be moved."));
        return false;
    }
    const QString resolvedPath = resolvePath(path);
    const QFileInfo sourceInfo(resolvedPath);
    if (resolvedPath.isEmpty() || !isWithinRoot(resolvedPath) || resolvedPath == m_rootPath
        || !sourceInfo.exists() || sourceInfo.isSymLink()) {
        setErrorMessage(QStringLiteral("That item cannot be moved safely."));
        return false;
    }

    m_clipboardPath = resolvedPath;
    m_clipboardOperation = QStringLiteral("cut");
    clearConflict();
    setTransferStatus(QStringLiteral("Ready to move %1.").arg(sourceInfo.fileName()), 0);
    setErrorMessage({});
    emit clipboardChanged();
    return true;
}

bool FileBrowserController::pasteClipboard()
{
    return pasteClipboard(QStringLiteral("ask"));
}

bool FileBrowserController::pasteClipboard(const QString &conflictResolution)
{
    if (m_transferActive) {
        setErrorMessage(QStringLiteral("Wait for the current transfer to finish."));
        return false;
    }
    if (!canPaste()) {
        setErrorMessage(QStringLiteral("Choose an item and a writable Home folder before pasting."));
        return false;
    }

    const QFileInfo sourceInfo(m_clipboardPath);
    QString destinationPath = normalizedPath(QDir(m_currentPath).filePath(sourceInfo.fileName()));
    if (!isWithinRoot(destinationPath) || destinationPath == m_clipboardPath
        || pathMatchesRoot(destinationPath, m_clipboardPath)) {
        setErrorMessage(QStringLiteral("That item cannot be pasted into itself."));
        return false;
    }

    const QString resolution = conflictResolution.trimmed().toLower();
    if (QFileInfo::exists(destinationPath)) {
        if (resolution == QStringLiteral("ask")) {
            m_conflictDestination = destinationPath;
            emit conflictChanged();
            setErrorMessage(QStringLiteral("An item named %1 already exists.").arg(sourceInfo.fileName()));
            return false;
        }
        if (resolution != QStringLiteral("keepboth")) {
            setErrorMessage(QStringLiteral("Choose Keep Both or cancel the transfer."));
            return false;
        }
        destinationPath = keepBothPath(destinationPath);
        if (destinationPath.isEmpty()) {
            setErrorMessage(QStringLiteral("Unable to choose a safe copy name."));
            return false;
        }
    }

    clearConflict();
    clearUndo();
    setTransferStatus(QStringLiteral("Transferring %1...").arg(sourceInfo.fileName()), 10);
    m_transferActive = true;
    emit transferChanged();
    emit clipboardChanged();
    const bool moving = m_clipboardOperation == QStringLiteral("cut");
    const QString sourcePath = m_clipboardPath;
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this,
            [this, watcher, moving, sourcePath, destinationPath]() {
        const bool succeeded = watcher->result();
        watcher->deleteLater();
        m_transferActive = false;
        if (!succeeded) {
            setTransferStatus(QStringLiteral("Transfer failed."), 0);
            setErrorMessage(QStringLiteral("Unable to transfer that item."));
            emit clipboardChanged();
            return;
        }

        m_undoSource = sourcePath;
        m_undoDestination = destinationPath;
        m_undoOperation = moving ? QStringLiteral("cut") : QStringLiteral("copy");
        emit undoChanged();
        if (moving) {
            m_clipboardPath.clear();
            m_clipboardOperation.clear();
        }
        setTransferStatus(QStringLiteral("%1 completed: %2")
                              .arg(moving ? QStringLiteral("Move") : QStringLiteral("Copy"),
                                   QFileInfo(destinationPath).fileName()),
                          100);
        setErrorMessage({});
        emit clipboardChanged();
        refresh();
    });
    watcher->setFuture(QtConcurrent::run([moving, sourcePath, destinationPath]() {
        return moving
            ? QFile::rename(sourcePath, destinationPath)
            : copyEntryRecursively(sourcePath, destinationPath);
    }));
    return true;
}

void FileBrowserController::cancelConflict()
{
    clearConflict();
    setErrorMessage({});
}

void FileBrowserController::clearClipboard()
{
    if (m_clipboardPath.isEmpty() && m_clipboardOperation.isEmpty()) {
        return;
    }
    m_clipboardPath.clear();
    m_clipboardOperation.clear();
    clearConflict();
    setTransferStatus({}, 0);
    emit clipboardChanged();
}

bool FileBrowserController::undoLastTransfer()
{
    if (m_transferActive) {
        setErrorMessage(QStringLiteral("Wait for the current transfer to finish."));
        return false;
    }
    if (!canUndo()) {
        setErrorMessage(QStringLiteral("There is no recent file transfer to undo."));
        return false;
    }

    const QString operation = m_undoOperation;
    bool succeeded = false;
    if (operation == QStringLiteral("copy")) {
        const QFileInfo copiedInfo(m_undoDestination);
        if (isWithinRoot(m_undoDestination) && copiedInfo.exists() && ensureTrashDirectories()) {
            const QString trashName = uniqueTrashName(copiedInfo.fileName());
            const QString trashPath = QDir(trashFilesPath()).filePath(trashName);
            const QString infoPath = QDir(trashInfoPath()).filePath(
                trashName + QStringLiteral(".trashinfo"));
            succeeded = !trashName.isEmpty()
                && QFile::rename(m_undoDestination, trashPath)
                && writeTrashInfo(infoPath, m_undoDestination);
            if (!succeeded && QFileInfo::exists(trashPath) && !QFileInfo::exists(m_undoDestination)) {
                QFile::rename(trashPath, m_undoDestination);
            }
        }
    } else if (operation == QStringLiteral("cut")) {
        succeeded = isWithinRoot(m_undoSource) && isWithinRoot(m_undoDestination)
            && !QFileInfo::exists(m_undoSource) && QFileInfo::exists(m_undoDestination)
            && QDir().mkpath(QFileInfo(m_undoSource).absolutePath())
            && QFile::rename(m_undoDestination, m_undoSource);
    }

    if (!succeeded) {
        setErrorMessage(QStringLiteral("The recent transfer changed and cannot be undone safely."));
        return false;
    }

    setTransferStatus(operation == QStringLiteral("copy")
                          ? QStringLiteral("Copy undone; the created item is in Trash.")
                          : QStringLiteral("Move undone."),
                      100);
    clearUndo();
    setErrorMessage({});
    refresh();
    return true;
}

void FileBrowserController::refresh()
{
    refreshDesktopEntries();

    if (searching()) {
        refreshSearchResults();
        return;
    }

    QVariantList refreshedEntries;
    const QString directoryPath = m_showingTrash ? trashFilesPath() : m_currentPath;
    const QDir directory(directoryPath);
    if (!directory.exists()) {
        setErrorMessage(QStringLiteral("Unable to read the current folder."));
        if (!m_entries.isEmpty()) {
            m_entries.clear();
            emit entriesChanged();
        }
        return;
    }

    const QFileInfoList fileInfos = directory.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::IgnoreCase | QDir::Name);
    for (const QFileInfo &info : fileInfos) {
        const QString resolvedPath = canonicalOrNormalizedPath(info.absoluteFilePath());
        if (resolvedPath.isEmpty()
            || (m_showingTrash ? !isWithinTrash(resolvedPath) : !isWithinNavigationRoot(resolvedPath))) {
            continue;
        }

        QVariantMap entry{
            {QStringLiteral("name"), info.fileName()},
            {QStringLiteral("path"), resolvedPath},
            {QStringLiteral("isDirectory"), info.isDir()},
            {QStringLiteral("kind"), info.isDir() ? QStringLiteral("Folder") : QStringLiteral("File")},
            {QStringLiteral("size"), info.isDir() ? qint64(0) : info.size()},
            {QStringLiteral("modified"), info.lastModified().toString(Qt::ISODate)},
            {QStringLiteral("readOnly"), readOnlyLocation()},
        };
        if (m_showingTrash) {
            QString originalPath;
            if (readTrashOriginalPath(resolvedPath, &originalPath)) {
                entry.insert(QStringLiteral("originalPath"), originalPath);
                entry.insert(QStringLiteral("originalLocation"),
                             originalPath.startsWith(m_rootPath + QLatin1Char('/'))
                                 ? QStringLiteral("~/") + originalPath.mid(m_rootPath.size() + 1)
                                 : originalPath);
            }
            entry.insert(QStringLiteral("isTrashEntry"), true);
        }
        refreshedEntries.append(entry);
    }

    sortEntries(&refreshedEntries);
    m_entries = refreshedEntries;
    emit entriesChanged();
    setErrorMessage({});
}

void FileBrowserController::setSortMode(const QString &mode)
{
    const QString normalizedMode = mode.trimmed().toLower();
    if (normalizedMode != QStringLiteral("name")
        && normalizedMode != QStringLiteral("type")
        && normalizedMode != QStringLiteral("size")
        && normalizedMode != QStringLiteral("modified")) {
        setErrorMessage(QStringLiteral("That Files sort mode is not available."));
        return;
    }

    if (m_sortMode == normalizedMode) {
        return;
    }

    m_sortMode = normalizedMode;
    emit sortChanged();
    refresh();
}

void FileBrowserController::toggleSortOrder()
{
    m_sortAscending = !m_sortAscending;
    emit sortChanged();
    refresh();
}

void FileBrowserController::refreshDesktopEntries()
{
    QVariantList refreshedEntries;
    const QString homePath = normalizedPath(m_rootPath);
    const QString desktopPath = normalizedPath(QDir(m_rootPath).filePath(QStringLiteral("Desktop")));
    if (m_desktopWatcher != nullptr && !m_desktopWatcher->directories().contains(homePath)) {
        m_desktopWatcher->addPath(homePath);
    }

    const QDir desktopDirectory(desktopPath);
    if (!desktopDirectory.exists()) {
        if (m_desktopWatcher != nullptr) {
            m_desktopWatcher->removePath(desktopPath);
        }
        if (!m_desktopEntries.isEmpty()) {
            m_desktopEntries.clear();
            emit desktopEntriesChanged();
        }
        return;
    }

    if (m_desktopWatcher != nullptr && !m_desktopWatcher->directories().contains(desktopPath)) {
        m_desktopWatcher->addPath(desktopPath);
    }

    const QFileInfoList fileInfos = desktopDirectory.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::IgnoreCase | QDir::Name);
    for (const QFileInfo &info : fileInfos) {
        const QString resolvedPath = canonicalOrNormalizedPath(info.absoluteFilePath());
        if (resolvedPath.isEmpty() || !isWithinRoot(resolvedPath)) {
            continue;
        }

        const bool launchable = isLaunchableDesktopEntry(info);
        refreshedEntries.append(QVariantMap{
            {QStringLiteral("name"), info.fileName()},
            {QStringLiteral("path"), resolvedPath},
            {QStringLiteral("isDirectory"), info.isDir() && !launchable},
            {QStringLiteral("isLaunchable"), launchable},
            {QStringLiteral("kind"), launchable
                    ? QStringLiteral("Application")
                    : (info.isDir() ? QStringLiteral("Folder") : QStringLiteral("File"))},
            {QStringLiteral("size"), info.isDir() ? qint64(0) : info.size()},
            {QStringLiteral("modified"), info.lastModified().toString(Qt::ISODate)},
            {QStringLiteral("readOnly"), false},
        });
    }

    if (m_desktopEntries == refreshedEntries) {
        return;
    }

    m_desktopEntries = refreshedEntries;
    emit desktopEntriesChanged();
}

void FileBrowserController::clearSearchQuery()
{
    if (m_searchQuery.isEmpty()) {
        return;
    }

    m_searchQuery.clear();
    emit searchQueryChanged();
}

void FileBrowserController::refreshSearchResults()
{
    struct SearchMatch {
        QFileInfo info;
        QString path;
    };

    const QString trashFilesRoot = normalizedPath(trashFilesPath());
    const QString trashInfoRoot = normalizedPath(trashInfoPath());
    QList<SearchMatch> matches;
    QDirIterator iterator(
        m_rootPath,
        QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        const QString candidatePath = iterator.next();
        const QString resolvedPath = canonicalOrNormalizedPath(candidatePath);
        if (resolvedPath.isEmpty()
            || !isWithinRoot(resolvedPath)
            || pathMatchesRoot(resolvedPath, trashFilesRoot)
            || pathMatchesRoot(resolvedPath, trashInfoRoot)) {
            continue;
        }

        const QFileInfo info(resolvedPath);
        if (!info.exists()) {
            continue;
        }

        const QString relativePath = QDir(m_rootPath).relativeFilePath(resolvedPath);
        if (!info.fileName().contains(m_searchQuery, Qt::CaseInsensitive)
            && !relativePath.contains(m_searchQuery, Qt::CaseInsensitive)) {
            continue;
        }

        matches.append({info, resolvedPath});
        if (matches.size() >= MaximumSearchResults) {
            break;
        }
    }

    std::sort(matches.begin(), matches.end(), [](const SearchMatch &left, const SearchMatch &right) {
        const bool leftDirectory = left.info.isDir();
        const bool rightDirectory = right.info.isDir();
        if (leftDirectory != rightDirectory) {
            return leftDirectory;
        }
        const int nameComparison = QString::compare(left.info.fileName(), right.info.fileName(), Qt::CaseInsensitive);
        if (nameComparison != 0) {
            return nameComparison < 0;
        }
        return QString::compare(left.path, right.path, Qt::CaseInsensitive) < 0;
    });

    QVariantList refreshedEntries;
    refreshedEntries.reserve(matches.size());
    for (const SearchMatch &match : std::as_const(matches)) {
        const QString relativePath = QDir(m_rootPath).relativeFilePath(match.path);
        refreshedEntries.append(QVariantMap{
            {QStringLiteral("name"), match.info.fileName()},
            {QStringLiteral("path"), match.path},
            {QStringLiteral("isDirectory"), match.info.isDir()},
            {QStringLiteral("kind"), match.info.isDir() ? QStringLiteral("Folder") : QStringLiteral("File")},
            {QStringLiteral("size"), match.info.isDir() ? qint64(0) : match.info.size()},
            {QStringLiteral("modified"), match.info.lastModified().toString(Qt::ISODate)},
            {QStringLiteral("searchLocation"), QStringLiteral("~/") + relativePath},
        });
    }

    sortEntries(&refreshedEntries);
    m_entries = refreshedEntries;
    emit entriesChanged();
    setErrorMessage({});
}

void FileBrowserController::sortEntries(QVariantList *entries) const
{
    if (entries == nullptr) {
        return;
    }

    std::stable_sort(entries->begin(), entries->end(), [this](const QVariant &left, const QVariant &right) {
        const QVariantMap leftMap = left.toMap();
        const QVariantMap rightMap = right.toMap();
        const bool leftDirectory = leftMap.value(QStringLiteral("isDirectory")).toBool();
        const bool rightDirectory = rightMap.value(QStringLiteral("isDirectory")).toBool();

        if (leftDirectory != rightDirectory) {
            return leftDirectory;
        }

        int comparison = 0;
        if (m_sortMode == QStringLiteral("type")) {
            comparison = QString::compare(
                leftMap.value(QStringLiteral("kind")).toString(),
                rightMap.value(QStringLiteral("kind")).toString(),
                Qt::CaseInsensitive);
        } else if (m_sortMode == QStringLiteral("size")) {
            const qint64 leftSize = leftMap.value(QStringLiteral("size")).toLongLong();
            const qint64 rightSize = rightMap.value(QStringLiteral("size")).toLongLong();
            comparison = leftSize < rightSize ? -1 : leftSize > rightSize ? 1 : 0;
        } else if (m_sortMode == QStringLiteral("modified")) {
            comparison = QString::compare(
                leftMap.value(QStringLiteral("modified")).toString(),
                rightMap.value(QStringLiteral("modified")).toString(),
                Qt::CaseInsensitive);
        } else {
            comparison = QString::compare(
                leftMap.value(QStringLiteral("name")).toString(),
                rightMap.value(QStringLiteral("name")).toString(),
                Qt::CaseInsensitive);
        }

        if (comparison == 0) {
            comparison = QString::compare(
                leftMap.value(QStringLiteral("name")).toString(),
                rightMap.value(QStringLiteral("name")).toString(),
                Qt::CaseInsensitive);
        }

        return m_sortAscending ? comparison < 0 : comparison > 0;
    });
}

QString FileBrowserController::normalizedPath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QString FileBrowserController::canonicalOrNormalizedPath(const QString &path)
{
    const QString normalized = normalizedPath(path);
    const QString canonical = QFileInfo(normalized).canonicalFilePath();
    return canonical.isEmpty() ? normalized : normalizedPath(canonical);
}

bool FileBrowserController::isValidEntryName(const QString &name)
{
    return !name.isEmpty()
        && name != QStringLiteral(".")
        && name != QStringLiteral("..")
        && !name.contains(QLatin1Char('/'))
        && !name.contains(QLatin1Char('\\'));
}

QString FileBrowserController::resolvePath(const QString &path) const
{
    if (path.trimmed().isEmpty()) {
        return {};
    }

    const QFileInfo pathInfo(path);
    const QString absolutePath = pathInfo.isAbsolute()
        ? path
        : QDir(m_currentPath).filePath(path);
    return canonicalOrNormalizedPath(absolutePath);
}

QString FileBrowserController::resolveTrashPath(const QString &path) const
{
    if (path.trimmed().isEmpty()) {
        return {};
    }

    const QFileInfo pathInfo(path);
    const QString absolutePath = pathInfo.isAbsolute()
        ? path
        : QDir(trashFilesPath()).filePath(path);
    return canonicalOrNormalizedPath(absolutePath);
}

bool FileBrowserController::isWithinRoot(const QString &path) const
{
    return pathMatchesRoot(normalizedPath(path), normalizedPath(m_rootPath));
}

bool FileBrowserController::isWithinNavigationRoot(const QString &path) const
{
    return pathMatchesRoot(normalizedPath(path), normalizedPath(m_navigationRoot));
}

bool FileBrowserController::isMountedLocationRoot(const QString &path) const
{
    const QString candidate = canonicalOrNormalizedPath(path);
    if (candidate.isEmpty()) {
        return false;
    }

    QStringList roots = m_mountedLocationRoots;
    if (roots.isEmpty()) {
        for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
            if (storage.isValid() && storage.isReady()) {
                roots.append(storage.rootPath());
            }
        }
    }

    for (const QString &root : std::as_const(roots)) {
        if (canonicalOrNormalizedPath(root) == candidate) {
            return true;
        }
    }
    return false;
}

bool FileBrowserController::isWithinTrash(const QString &path) const
{
    return pathMatchesRoot(normalizedPath(path), normalizedPath(trashFilesPath()));
}

QString FileBrowserController::trashFilesPath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (dataPath.isEmpty() || !isWithinRoot(canonicalOrNormalizedPath(dataPath))) {
        dataPath = QDir(m_rootPath).filePath(QStringLiteral(".local/share"));
    }
    return normalizedPath(QDir(dataPath).filePath(QStringLiteral("Trash/files")));
}

QString FileBrowserController::trashInfoPath() const
{
    return normalizedPath(QDir(trashFilesPath()).filePath(QStringLiteral("../info")));
}

QString FileBrowserController::uniqueTrashName(const QString &name) const
{
    const QDir filesDirectory(trashFilesPath());
    const QDir infoDirectory(trashInfoPath());
    QString candidate = name;
    int suffix = 2;
    while (QFileInfo::exists(filesDirectory.filePath(candidate))
           || QFileInfo::exists(infoDirectory.filePath(candidate + QStringLiteral(".trashinfo")))) {
        candidate = QStringLiteral("%1 (%2)").arg(name).arg(suffix++);
    }
    return candidate;
}

bool FileBrowserController::ensureTrashDirectories() const
{
    return QDir().mkpath(trashFilesPath()) && QDir().mkpath(trashInfoPath());
}

bool FileBrowserController::readTrashOriginalPath(const QString &trashPath, QString *originalPath) const
{
    if (originalPath == nullptr || !isWithinTrash(trashPath)) {
        return false;
    }

    const QFileInfo trashInfo(trashPath);
    QFile metadata(QDir(trashInfoPath()).filePath(trashInfo.fileName() + QStringLiteral(".trashinfo")));
    if (!metadata.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray encodedPath;
    const QList<QByteArray> lines = metadata.readAll().split('\n');
    for (const QByteArray &line : lines) {
        if (line.startsWith("Path=")) {
            encodedPath = line.mid(5).trimmed();
            break;
        }
    }
    if (encodedPath.isEmpty()) {
        return false;
    }

    const QString resolvedOriginalPath = canonicalOrNormalizedPath(QUrl::fromPercentEncoding(encodedPath));
    if (resolvedOriginalPath.isEmpty() || !isWithinRoot(resolvedOriginalPath)) {
        return false;
    }
    *originalPath = resolvedOriginalPath;
    return true;
}

bool FileBrowserController::writeTrashInfo(const QString &infoPath, const QString &originalPath) const
{
    QSaveFile infoFile(infoPath);
    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    const QByteArray encodedPath = QUrl::toPercentEncoding(originalPath);
    const QByteArray deletionDate = QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8();
    const QByteArray contents = QByteArrayLiteral("[Trash Info]\nPath=")
        + encodedPath
        + QByteArrayLiteral("\nDeletionDate=")
        + deletionDate
        + QByteArrayLiteral("\n");
    if (infoFile.write(contents) != contents.size()) {
        return false;
    }
    return infoFile.commit();
}

bool FileBrowserController::isAllowedClipboardSource(const QString &path) const
{
    if (isWithinRoot(path)) {
        return true;
    }
    for (const QString &root : m_mountedLocationRoots) {
        if (pathMatchesRoot(normalizedPath(path), canonicalOrNormalizedPath(root))) {
            return true;
        }
    }
    return false;
}

QString FileBrowserController::keepBothPath(const QString &destinationPath) const
{
    const QFileInfo destinationInfo(destinationPath);
    const QString directoryPath = destinationInfo.absolutePath();
    const bool directory = destinationInfo.isDir();
    const QString suffix = directory || destinationInfo.suffix().isEmpty()
        ? QString() : QStringLiteral(".") + destinationInfo.suffix();
    const QString baseName = suffix.isEmpty()
        ? destinationInfo.fileName() : destinationInfo.completeBaseName();
    for (int copyNumber = 1; copyNumber <= 999; ++copyNumber) {
        const QString copyLabel = copyNumber == 1
            ? QStringLiteral(" copy")
            : QStringLiteral(" copy %1").arg(copyNumber);
        const QString candidate = normalizedPath(
            QDir(directoryPath).filePath(baseName + copyLabel + suffix));
        if (isWithinRoot(candidate) && !QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

void FileBrowserController::clearConflict()
{
    if (m_conflictDestination.isEmpty()) {
        return;
    }
    m_conflictDestination.clear();
    emit conflictChanged();
}

void FileBrowserController::clearUndo()
{
    if (m_undoOperation.isEmpty() && m_undoSource.isEmpty() && m_undoDestination.isEmpty()) {
        return;
    }
    m_undoSource.clear();
    m_undoDestination.clear();
    m_undoOperation.clear();
    emit undoChanged();
}

void FileBrowserController::setTransferStatus(const QString &status, int progress)
{
    const int boundedProgress = std::clamp(progress, 0, 100);
    if (m_transferStatus == status && m_transferProgress == boundedProgress) {
        return;
    }
    m_transferStatus = status;
    m_transferProgress = boundedProgress;
    emit transferChanged();
}

void FileBrowserController::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }

    m_errorMessage = message;
    emit errorMessageChanged();
}
