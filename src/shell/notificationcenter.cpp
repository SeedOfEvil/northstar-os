#include "notificationcenter.h"

#include <QDateTime>
#include <QVariantMap>

NotificationCenter::NotificationCenter(QObject *parent, int maxNotifications)
    : QObject(parent)
    , m_maxNotifications(qMax(1, maxNotifications))
{
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
    entry.kind = kind.trimmed().toLower();
    if (entry.kind != QStringLiteral("success")
        && entry.kind != QStringLiteral("warning")
        && entry.kind != QStringLiteral("error")) {
        entry.kind = QStringLiteral("info");
    }
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
    emit notificationsChanged();
    if (hadUnread) {
        emit unreadCountChanged();
    }
}

QVariantList NotificationCenter::toVariantList(const QList<NotificationEntry> &entries)
{
    QVariantList result;
    result.reserve(entries.size());
    for (const NotificationEntry &entry : entries) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), entry.id);
        item.insert(QStringLiteral("title"), entry.title);
        item.insert(QStringLiteral("body"), entry.body);
        item.insert(QStringLiteral("kind"), entry.kind);
        item.insert(QStringLiteral("timestamp"), entry.timestamp);
        item.insert(QStringLiteral("read"), entry.read);
        result.append(item);
    }
    return result;
}
