#include "filebrowsercontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>

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

void FileBrowserController::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }

    m_errorMessage = message;
    emit errorMessageChanged();
}
