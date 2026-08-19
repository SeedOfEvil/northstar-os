#pragma once

#include <QDateTime>
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
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY doNotDisturbChanged)

public:
    // An empty storePath means the account's own history file. Tests pass a
    // temporary path so they never read or write the real desktop's history.
    explicit NotificationCenter(QObject *parent = nullptr,
                                int maxNotifications = 40,
                                QString storePath = {});

    // The only kinds the panel knows how to colour. Anything else becomes
    // "info", whether it arrives from a producer or from a hand-edited file.
    static QString normalizedKind(const QString &kind);

    // Short, human-readable age used by the panel. History now survives a
    // restart, so a stored ISO timestamp on its own reads badly.
    static QString relativeTime(const QDateTime &when, const QDateTime &now);

    QString storePath() const;
    QList<NotificationEntry> entries() const;
    QVariantList notifications() const;
    int unreadCount() const;
    bool doNotDisturb() const;

    Q_INVOKABLE QString pushNotification(const QString &title,
                                         const QString &body,
                                         const QString &kind = QStringLiteral("info"));
    Q_INVOKABLE bool markRead(const QString &id);
    Q_INVOKABLE void markAllRead();
    Q_INVOKABLE bool dismissNotification(const QString &id);
    Q_INVOKABLE void clearNotifications();

public slots:
    void setDoNotDisturb(bool enabled);

signals:
    void notificationsChanged();
    void unreadCountChanged();
    void doNotDisturbChanged();

private:
    static QVariantList toVariantList(const QList<NotificationEntry> &entries);
    void persist() const;

    QList<NotificationEntry> m_entries;
    QString m_storePath;
    int m_maxNotifications = 40;
    int m_nextId = 1;
    bool m_doNotDisturb = false;
};
