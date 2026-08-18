#include "recentfilesstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr int MaximumRecentEntries = 12;

QString defaultSettingsPath()
{
    QString configDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDirectory.isEmpty()) {
        configDirectory = QDir::home().filePath(QStringLiteral(".config/northstar"));
    }
    return QDir(configDirectory).filePath(QStringLiteral("text-editor-recent.ini"));
}

QString normalizedPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    const QFileInfo info(QDir::cleanPath(QDir::fromNativeSeparators(trimmed)));
    return info.isAbsolute() ? info.absoluteFilePath() : QString();
}

} // namespace

RecentFilesStore::RecentFilesStore(QString settingsPath)
    : m_settingsPath(settingsPath.trimmed().isEmpty()
            ? defaultSettingsPath()
            : QDir::cleanPath(QDir::fromNativeSeparators(settingsPath)))
{
    load();
}

int RecentFilesStore::maximumEntries()
{
    return MaximumRecentEntries;
}

QString RecentFilesStore::settingsPath() const
{
    return m_settingsPath;
}

QStringList RecentFilesStore::paths() const
{
    return m_paths;
}

void RecentFilesStore::remember(const QString &path)
{
    const QString normalized = normalizedPath(path);
    if (normalized.isEmpty()) {
        return;
    }

    m_paths.removeAll(normalized);
    m_paths.prepend(normalized);
    while (m_paths.size() > MaximumRecentEntries) {
        m_paths.removeLast();
    }
    store();
}

bool RecentFilesStore::forget(const QString &path)
{
    const QString normalized = normalizedPath(path);
    if (normalized.isEmpty() || !m_paths.removeOne(normalized)) {
        return false;
    }
    return store();
}

void RecentFilesStore::clear()
{
    if (m_paths.isEmpty()) {
        return;
    }
    m_paths.clear();
    store();
}

void RecentFilesStore::load()
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    const QStringList recorded = settings.value(QStringLiteral("recent/paths")).toStringList();
    for (const QString &candidate : recorded) {
        const QString normalized = normalizedPath(candidate);
        if (normalized.isEmpty() || m_paths.contains(normalized)) {
            continue;
        }
        m_paths.append(normalized);
        if (m_paths.size() >= MaximumRecentEntries) {
            break;
        }
    }
}

bool RecentFilesStore::store() const
{
    const QFileInfo settingsInfo(m_settingsPath);
    if (!QDir().mkpath(settingsInfo.absolutePath())) {
        return false;
    }

    QSettings settings(m_settingsPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("recent/paths"), m_paths);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    // The history names the user's own documents; keep it owner-readable only.
    QFile::setPermissions(m_settingsPath, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}
