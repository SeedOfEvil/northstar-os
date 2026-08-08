#include "filebrowsercontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QDateTime>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace {

bool pathMatchesRoot(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root + QLatin1Char('/'));
}

} // namespace

FileBrowserController::FileBrowserController(QObject *parent,
                                             QString rootPath,
                                             OpenFunction openFunction)
    : QObject(parent)
    , m_rootPath(canonicalOrNormalizedPath(rootPath.isEmpty() ? QDir::homePath() : rootPath))
    , m_currentPath(m_rootPath)
    , m_openFunction(std::move(openFunction))
{
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
    if (m_currentPath == m_rootPath) {
        return QStringLiteral("~");
    }

    const QString prefix = m_rootPath + QLatin1Char('/');
    if (m_currentPath.startsWith(prefix)) {
        return QStringLiteral("~/") + m_currentPath.mid(prefix.size());
    }
    return m_currentPath;
}

QString FileBrowserController::homePath() const
{
    return m_rootPath;
}

QString FileBrowserController::errorMessage() const
{
    return m_errorMessage;
}

bool FileBrowserController::navigateTo(const QString &path)
{
    const QString resolvedPath = resolvePath(path);
    if (resolvedPath.isEmpty() || !isWithinRoot(resolvedPath)) {
        setErrorMessage(QStringLiteral("That location is outside the Northstar home folder."));
        return false;
    }

    const QFileInfo info(resolvedPath);
    if (!info.exists() || !info.isDir()) {
        setErrorMessage(QStringLiteral("That folder is not available."));
        return false;
    }

    if (m_currentPath != resolvedPath) {
        m_currentPath = resolvedPath;
        emit currentPathChanged();
    }
    setErrorMessage({});
    refresh();
    return true;
}

bool FileBrowserController::navigateUp()
{
    if (m_currentPath == m_rootPath) {
        return false;
    }

    const QFileInfo currentInfo(m_currentPath);
    return navigateTo(currentInfo.dir().absolutePath());
}

bool FileBrowserController::goHome()
{
    return navigateTo(m_rootPath);
}

bool FileBrowserController::openEntry(const QString &path)
{
    const QString resolvedPath = resolvePath(path);
    if (resolvedPath.isEmpty() || !isWithinRoot(resolvedPath)) {
        setErrorMessage(QStringLiteral("That item is outside the Northstar home folder."));
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

bool FileBrowserController::renameEntry(const QString &path, const QString &newName)
{
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

void FileBrowserController::refresh()
{
    QVariantList refreshedEntries;
    const QDir directory(m_currentPath);
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

bool FileBrowserController::isWithinRoot(const QString &path) const
{
    return pathMatchesRoot(normalizedPath(path), normalizedPath(m_rootPath));
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
