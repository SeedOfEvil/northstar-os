#include "notificationcenter.h"

#include "notificationstore.h"

#include <QDateTime>
#include <QLocale>
#include <QVariantMap>

#include <utility>

namespace {

const QLatin1String IdPrefix("notification-");

// Restored entries keep the ids they were dismissed by, so the next id has to
// clear every one of them or a fresh notification could collide with history.
int nextIdAfter(const QList<NotificationEntry> &entries)
{
    int highest = 0;
    for (const NotificationEntry &entry : entries) {
        if (!entry.id.startsWith(IdPrefix)) {
            continue;
        }
        bool numeric = false;
        const int value = entry.id.mid(IdPrefix.size()).toInt(&numeric);
        if (numeric && value > highest) {
            highest = value;
        }
    }
    return highest + 1;
}

} // namespace

NotificationCenter::NotificationCenter(QObject *parent, int maxNotifications, QString storePath)
    : QObject(parent)
    , m_storePath(NotificationStore(std::move(storePath)).settingsPath())
    , m_maxNotifications(qMax(1, maxNotifications))
{
    m_entries = NotificationStore(m_storePath).load(m_maxNotifications);
    m_nextId = nextIdAfter(m_entries);
}

QString NotificationCenter::normalizedKind(const QString &kind)
{
    const QString normalized = kind.trimmed().toLower();
    if (normalized == QStringLiteral("success") || normalized == QStringLiteral("warning")
        || normalized == QStringLiteral("error")) {
        return normalized;
    }
    return QStringLiteral("info");
}

QString NotificationCenter::relativeTime(const QDateTime &when, const QDateTime &now)
{
    if (!when.isValid()) {
        return {};
    }

    // A history file carried across a clock change can be stamped ahead of the
    // current time; treat that as the present rather than a negative age.
    const qint64 seconds = qMax<qint64>(0, when.secsTo(now));
    if (seconds < 60) {
        return QStringLiteral("Just now");
    }
    if (seconds < 3600) {
        return QStringLiteral("%1m ago").arg(seconds / 60);
    }
    if (seconds < 86400) {
        return QStringLiteral("%1h ago").arg(seconds / 3600);
    }
    if (seconds < 7 * 86400) {
        const qint64 days = seconds / 86400;
        return days == 1 ? QStringLiteral("Yesterday") : QStringLiteral("%1d ago").arg(days);
    }
    return QLocale().toString(when, QLocale::ShortFormat);
}

QString NotificationCenter::storePath() const
{
    return m_storePath;
}

void NotificationCenter::persist() const
{
    NotificationStore(m_storePath).save(m_entries);
}

QList<NotificationEntry> NotificationCenter::entries() const
{
    return m_entries;
}

QVariantList NotificationCenter::notifications() const
{
    return toVariantList(m_entries);
}

int NotificationCenter::unreadCount() const
{
    int count = 0;
    for (const NotificationEntry &entry : m_entries) {
        if (!entry.read) {
            ++count;
        }
    }
    return count;
}

bool NotificationCenter::doNotDisturb() const
{
    return m_doNotDisturb;
}

QString NotificationCenter::pushNotification(const QString &title,
                                              const QString &body,
                                              const QString &kind)
{
    NotificationEntry entry;
    entry.id = QStringLiteral("notification-%1").arg(m_nextId++);
    entry.title = title.trimmed().left(120);
    entry.body = body.trimmed().left(500);
    entry.kind = normalizedKind(kind);
    if (entry.title.isEmpty()) {
        entry.title = QStringLiteral("Northstar");
    }
    if (entry.body.isEmpty()) {
        entry.body = entry.title;
    }
    entry.timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    entry.read = m_doNotDisturb;

    const int previousUnreadCount = unreadCount();
    m_entries.prepend(entry);
    while (m_entries.size() > m_maxNotifications) {
        m_entries.removeLast();
    }

    persist();
    emit notificationsChanged();
    if (previousUnreadCount != unreadCount()) {
        emit unreadCountChanged();
    }
    return entry.id;
}

void NotificationCenter::setDoNotDisturb(bool enabled)
{
    if (m_doNotDisturb == enabled) {
        return;
    }
    m_doNotDisturb = enabled;
    emit doNotDisturbChanged();
}

bool NotificationCenter::markRead(const QString &id)
{
    const QString normalizedId = id.trimmed();
    for (NotificationEntry &entry : m_entries) {
        if (entry.id != normalizedId || entry.read) {
            continue;
        }
        entry.read = true;
        persist();
        emit notificationsChanged();
        emit unreadCountChanged();
        return true;
    }
    return false;
}

void NotificationCenter::markAllRead()
{
    bool changed = false;
    for (NotificationEntry &entry : m_entries) {
        if (!entry.read) {
            entry.read = true;
            changed = true;
        }
    }
    if (!changed) {
        return;
    }

    persist();
    emit notificationsChanged();
    emit unreadCountChanged();
}

bool NotificationCenter::dismissNotification(const QString &id)
{
    const QString normalizedId = id.trimmed();
    for (qsizetype index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).id != normalizedId) {
            continue;
        }

        const int previousUnreadCount = unreadCount();
        m_entries.removeAt(index);
        persist();
        emit notificationsChanged();
        if (previousUnreadCount != unreadCount()) {
            emit unreadCountChanged();
        }
        return true;
    }
    return false;
}

void NotificationCenter::clearNotifications()
{
    if (m_entries.isEmpty()) {
        return;
    }

    const bool hadUnread = unreadCount() > 0;
    m_entries.clear();
    persist();
    emit notificationsChanged();
    if (hadUnread) {
        emit unreadCountChanged();
    }
}

QVariantList NotificationCenter::toVariantList(const QList<NotificationEntry> &entries)
{
    QVariantList result;
    result.reserve(entries.size());
    const QDateTime now = QDateTime::currentDateTime();
    for (const NotificationEntry &entry : entries) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), entry.id);
        item.insert(QStringLiteral("title"), entry.title);
        item.insert(QStringLiteral("body"), entry.body);
        item.insert(QStringLiteral("kind"), entry.kind);
        item.insert(QStringLiteral("timestamp"), entry.timestamp);
        item.insert(QStringLiteral("displayTime"),
                    relativeTime(QDateTime::fromString(entry.timestamp, Qt::ISODateWithMs), now));
        item.insert(QStringLiteral("read"), entry.read);
        result.append(item);
    }
    return result;
}
