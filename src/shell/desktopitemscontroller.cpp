#include "desktopitemscontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

#include <algorithm>

#include <utility>

namespace {

QString itemKind(const QFileInfo &info, bool launchable)
{
    if (launchable) {
        return QStringLiteral("Application");
    }
    return info.isDir() ? QStringLiteral("Folder") : QStringLiteral("File");
}

} // namespace

DesktopItemsController::DesktopItemsController(QObject *parent, QString homePath)
    : QObject(parent)
    , m_homePath(canonicalOrNormalizedPath(homePath.isEmpty() ? QDir::homePath() : homePath))
    , m_desktopPath(normalizedPath(QDir(m_homePath).filePath(QStringLiteral("Desktop"))))
    , m_watcher(new QFileSystemWatcher(this))
    , m_refreshTimer(new QTimer(this))
{
    m_refreshTimer->setInterval(100);
    m_refreshTimer->setSingleShot(true);
    connect(m_refreshTimer, &QTimer::timeout, this, &DesktopItemsController::refresh);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &DesktopItemsController::scheduleRefresh);
    refresh();
}

QVariantList DesktopItemsController::entries() const
{
    return m_entries;
}

QString DesktopItemsController::desktopPath() const
{
    return m_desktopPath;
}

QString DesktopItemsController::errorMessage() const
{
    return m_errorMessage;
}

bool DesktopItemsController::available() const
{
    return m_available;
}

void DesktopItemsController::refresh()
{
    const QFileInfo desktopInfo(m_desktopPath);
    const QString resolvedDesktopPath = desktopInfo.exists()
        ? canonicalOrNormalizedPath(m_desktopPath) : QString();
    const bool desktopAvailable = desktopInfo.exists()
        && desktopInfo.isDir()
        && !resolvedDesktopPath.isEmpty()
        && isWithinHome(resolvedDesktopPath);

    QVariantList refreshedEntries;
    if (desktopAvailable) {
        const QDir desktopDirectory(resolvedDesktopPath);
        const QFileInfoList fileInfos = desktopDirectory.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot,
            QDir::DirsFirst | QDir::IgnoreCase | QDir::Name);
        for (const QFileInfo &info : fileInfos) {
            const QString resolvedPath = canonicalOrNormalizedPath(info.absoluteFilePath());
            if (resolvedPath.isEmpty() || !isWithinHome(resolvedPath)) {
                continue;
            }

            const bool launchable = isLaunchable(resolvedPath, info);
            refreshedEntries.append(QVariantMap{
                {QStringLiteral("name"), info.fileName()},
                {QStringLiteral("path"), resolvedPath},
                {QStringLiteral("isDirectory"), info.isDir() && !launchable},
                {QStringLiteral("isLaunchable"), launchable},
                {QStringLiteral("kind"), itemKind(info, launchable)},
                {QStringLiteral("size"), info.isDir() ? qint64(0) : info.size()},
                {QStringLiteral("modified"), info.lastModified().toString(Qt::ISODate)},
            });
        }
        setErrorMessage({});
    } else if (desktopInfo.exists()) {
        setErrorMessage(QStringLiteral("The Desktop folder is outside the Northstar home folder."));
    } else {
        setErrorMessage({});
    }

    const bool entriesDiffer = m_entries != refreshedEntries;
    const bool availabilityChanged = m_available != desktopAvailable;
    m_available = desktopAvailable;
    m_entries = std::move(refreshedEntries);
    refreshWatcher();

    if (availabilityChanged) {
        emit availableChanged();
    }
    if (entriesDiffer) {
        emit entriesChanged();
    }
}

bool DesktopItemsController::requestOpen(const QString &path)
{
    QString resolvedPath;
    QFileInfo info;
    if (!resolveDesktopItem(path, &resolvedPath, &info)) {
        setErrorMessage(QStringLiteral("That desktop item is no longer available."));
        return false;
    }

    const bool launchable = isLaunchable(resolvedPath, info);
    emit openPathRequested(resolvedPath, info.isDir() && !launchable, launchable);
    return true;
}

bool DesktopItemsController::requestOpenWith(const QString &path)
{
    QString resolvedPath;
    QFileInfo info;
    if (!resolveDesktopItem(path, &resolvedPath, &info) || info.isDir()) {
        setErrorMessage(QStringLiteral("Open With is available for files on the Desktop."));
        return false;
    }

    emit openWithRequested(resolvedPath);
    return true;
}

QString DesktopItemsController::normalizedPath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QString DesktopItemsController::canonicalOrNormalizedPath(const QString &path)
{
    const QString normalized = normalizedPath(path);
    const QString canonical = QFileInfo(normalized).canonicalFilePath();
    return canonical.isEmpty() ? normalized : normalizedPath(canonical);
}

bool DesktopItemsController::pathMatchesRoot(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root + QLatin1Char('/'));
}

bool DesktopItemsController::isLaunchable(const QString &path, const QFileInfo &info)
{
    Q_UNUSED(path);
    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("desktop") || suffix == QStringLiteral("app");
}

bool DesktopItemsController::isWithinHome(const QString &path) const
{
    return pathMatchesRoot(normalizedPath(path), normalizedPath(m_homePath));
}

bool DesktopItemsController::resolveDesktopItem(const QString &path,
                                                QString *resolvedPath,
                                                QFileInfo *info) const
{
    if (resolvedPath == nullptr || info == nullptr || path.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo candidateInfo(path);
    const QString candidatePath = candidateInfo.isAbsolute()
        ? path : QDir(m_desktopPath).filePath(path);
    const QString resolved = canonicalOrNormalizedPath(candidatePath);
    if (resolved.isEmpty() || !isWithinHome(resolved)
        || !pathMatchesRoot(resolved, canonicalOrNormalizedPath(m_desktopPath))) {
        return false;
    }

    const QFileInfo resolvedInfo(resolved);
    if (!resolvedInfo.exists()) {
        return false;
    }

    *resolvedPath = resolved;
    *info = resolvedInfo;
    return true;
}

void DesktopItemsController::refreshWatcher()
{
    const QStringList watchedPaths = m_watcher->directories();
    if (!watchedPaths.isEmpty()) {
        m_watcher->removePaths(watchedPaths);
    }

    if (QFileInfo(m_homePath).isDir()) {
        m_watcher->addPath(m_homePath);
    }
    if (m_available) {
        m_watcher->addPath(m_desktopPath);
    }
}

void DesktopItemsController::scheduleRefresh()
{
    if (!m_refreshTimer->isActive()) {
        m_refreshTimer->start();
    }
}

void DesktopItemsController::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }

    m_errorMessage = message;
    emit errorMessageChanged();
}
