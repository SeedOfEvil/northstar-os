#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

struct NotificationEntry
{
    QString id;
    QString title;
    QString body;
    QString kind;
    QString timestamp;
    bool read = false;
};

inline bool operator==(const NotificationEntry &left, const NotificationEntry &right)
{
    return left.id == right.id
        && left.title == right.title
        && left.body == right.body
        && left.kind == right.kind
        && left.timestamp == right.timestamp
        && left.read == right.read;
}

class NotificationCenter final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)

public:
    explicit NotificationCenter(QObject *parent = nullptr, int maxNotifications = 40);

    QList<NotificationEntry> entries() const;
    QVariantList notifications() const;
    int unreadCount() const;

    Q_INVOKABLE QString pushNotification(const QString &title,
                                         const QString &body,
                                         const QString &kind = QStringLiteral("info"));
    Q_INVOKABLE bool markRead(const QString &id);
    Q_INVOKABLE void markAllRead();
    Q_INVOKABLE bool dismissNotification(const QString &id);
    Q_INVOKABLE void clearNotifications();

signals:
    void notificationsChanged();
    void unreadCountChanged();

private:
    static QVariantList toVariantList(const QList<NotificationEntry> &entries);

    QList<NotificationEntry> m_entries;
    int m_maxNotifications = 40;
    int m_nextId = 1;
};
