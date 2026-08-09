#include "filebrowsercontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QDateTime>
#include <QDirIterator>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>

#include <algorithm>

#include <utility>

namespace {

constexpr qsizetype MaximumSearchResults = 500;

bool pathMatchesRoot(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root + QLatin1Char('/'));
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

QVariantList FileBrowserController::desktopEntries() const
{
    return m_desktopEntries;
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

    m_entries = refreshedEntries;
    emit entriesChanged();
    setErrorMessage({});
}

void FileBrowserController::refreshDesktopEntries()
{
    QVariantList refreshedEntries;
    const QString desktopPath = normalizedPath(QDir(m_rootPath).filePath(QStringLiteral("Desktop")));
    const QDir desktopDirectory(desktopPath);
    if (!desktopDirectory.exists()) {
        if (m_desktopWatcher != nullptr) {
            m_desktopWatcher->removePaths(m_desktopWatcher->directories());
            m_desktopWatcher->removePaths(m_desktopWatcher->files());
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

        refreshedEntries.append(QVariantMap{
            {QStringLiteral("name"), info.fileName()},
            {QStringLiteral("path"), resolvedPath},
            {QStringLiteral("isDirectory"), info.isDir()},
            {QStringLiteral("kind"), info.isDir() ? QStringLiteral("Folder") : QStringLiteral("File")},
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

    m_entries = refreshedEntries;
    emit entriesChanged();
    setErrorMessage({});
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

void FileBrowserController::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }

    m_errorMessage = message;
    emit errorMessageChanged();
}
