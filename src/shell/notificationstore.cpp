#include "notificationstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr int RetentionDays = 14;

QString settingsGroup()
{
    return QStringLiteral("notifications");
}

} // namespace

NotificationStore::NotificationStore(QString settingsPath)
    : m_settingsPath(settingsPath.trimmed().isEmpty()
            ? defaultSettingsPath()
            : QDir::cleanPath(QDir::fromNativeSeparators(settingsPath)))
{
}

QString NotificationStore::defaultSettingsPath()
{
    QString configDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDirectory.isEmpty()) {
        configDirectory = QDir::home().filePath(QStringLiteral(".config/northstar"));
    }
    return QDir(configDirectory).filePath(QStringLiteral("notifications.ini"));
}

int NotificationStore::retentionDays()
{
    return RetentionDays;
}

QString NotificationStore::settingsPath() const
{
    return m_settingsPath;
}

QList<NotificationEntry> NotificationStore::load(int maxEntries) const
{
    QList<NotificationEntry> entries;
    const int limit = qMax(1, maxEntries);
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime oldestKept = now.addDays(-RetentionDays);

    QSettings settings(m_settingsPath, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        return entries;
    }

    const int recorded = settings.beginReadArray(settingsGroup());
    for (int index = 0; index < recorded && entries.size() < limit; ++index) {
        settings.setArrayIndex(index);

        NotificationEntry entry;
        entry.id = settings.value(QStringLiteral("id")).toString().trimmed();
        entry.title = settings.value(QStringLiteral("title")).toString().trimmed().left(120);
        entry.body = settings.value(QStringLiteral("body")).toString().trimmed().left(500);
        entry.kind = NotificationCenter::normalizedKind(
            settings.value(QStringLiteral("kind")).toString());
        entry.read = settings.value(QStringLiteral("read"), false).toBool();

        const QString timestamp = settings.value(QStringLiteral("timestamp")).toString().trimmed();
        const QDateTime recordedAt = QDateTime::fromString(timestamp, Qt::ISODateWithMs);

        // A record without an identity or a readable time cannot be dismissed
        // or ordered, so it is dropped rather than shown as a broken row.
        if (entry.id.isEmpty() || entry.title.isEmpty() || !recordedAt.isValid()) {
            continue;
        }
        if (recordedAt < oldestKept || recordedAt > now.addDays(1)) {
            continue;
        }

        entry.timestamp = timestamp;
        entries.append(entry);
    }
    settings.endArray();

    return entries;
}

bool NotificationStore::save(const QList<NotificationEntry> &entries) const
{
    const QFileInfo settingsInfo(m_settingsPath);
    if (!QDir().mkpath(settingsInfo.absolutePath())) {
        return false;
    }

    QSettings settings(m_settingsPath, QSettings::IniFormat);

    // beginWriteArray only truncates down to the new size, so an explicit
    // remove is what actually clears a history that shrank.
    settings.remove(settingsGroup());
    settings.beginWriteArray(settingsGroup(), static_cast<int>(entries.size()));
    for (qsizetype index = 0; index < entries.size(); ++index) {
        const NotificationEntry &entry = entries.at(index);
        settings.setArrayIndex(static_cast<int>(index));
        settings.setValue(QStringLiteral("id"), entry.id);
        settings.setValue(QStringLiteral("title"), entry.title);
        settings.setValue(QStringLiteral("body"), entry.body);
        settings.setValue(QStringLiteral("kind"), entry.kind);
        settings.setValue(QStringLiteral("timestamp"), entry.timestamp);
        settings.setValue(QStringLiteral("read"), entry.read);
    }
    settings.endArray();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    // The history names the applications this account runs; keep it readable
    // by its owner alone.
    QFile::setPermissions(m_settingsPath, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}
