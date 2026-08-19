#include "notificationcenter.h"

#include <QDateTime>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QtTest/QtTest>

class NotificationCenterTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void pushesNewestUnreadNotification();
    void marksAndDismissesNotifications();
    void capsNotificationHistory();
    void doNotDisturbKeepsNewNotificationsRead();
    void restoresHistoryAcrossARestart();
    void neverReissuesARestoredIdentifier();
    void persistsDismissalAndClearing();
    void restoresNoMoreThanTheHistoryCap();
    void describesEntryAgeInWords();
    void offersADisplayTimeToTheView();

private:
    // Every case gets its own history file so no test reads or writes the
    // account's real desktop history.
    QString storePath(const QString &name = QStringLiteral("history.ini")) const;

    QTemporaryDir *m_directory = nullptr;
};

void NotificationCenterTest::init()
{
    m_directory = new QTemporaryDir;
    QVERIFY(m_directory->isValid());
}

void NotificationCenterTest::cleanup()
{
    delete m_directory;
    m_directory = nullptr;
}

QString NotificationCenterTest::storePath(const QString &name) const
{
    return m_directory->filePath(name);
}

void NotificationCenterTest::pushesNewestUnreadNotification()
{
    NotificationCenter center(nullptr, 40, storePath());

    const QString id = center.pushNotification(
        QStringLiteral("Application started"),
        QStringLiteral("Started Firefox"),
        QStringLiteral("success"));

    QVERIFY(!id.isEmpty());
    QCOMPARE(center.unreadCount(), 1);
    QCOMPARE(center.notifications().size(), 1);
    const QVariantMap item = center.notifications().first().toMap();
    QCOMPARE(item.value(QStringLiteral("id")).toString(), id);
    QCOMPARE(item.value(QStringLiteral("title")).toString(), QStringLiteral("Application started"));
    QCOMPARE(item.value(QStringLiteral("body")).toString(), QStringLiteral("Started Firefox"));
    QCOMPARE(item.value(QStringLiteral("kind")).toString(), QStringLiteral("success"));
    QVERIFY(!item.value(QStringLiteral("read")).toBool());
}

void NotificationCenterTest::marksAndDismissesNotifications()
{
    NotificationCenter center(nullptr, 40, storePath());
    const QString firstId = center.pushNotification(QStringLiteral("First"), QStringLiteral("One"));
    const QString secondId = center.pushNotification(QStringLiteral("Second"), QStringLiteral("Two"));

    QVERIFY(center.markRead(firstId));
    QCOMPARE(center.unreadCount(), 1);
    QVERIFY(!center.markRead(firstId));

    center.markAllRead();
    QCOMPARE(center.unreadCount(), 0);
    QVERIFY(center.dismissNotification(secondId));
    QCOMPARE(center.notifications().size(), 1);
    QVERIFY(!center.dismissNotification(secondId));

    center.clearNotifications();
    QVERIFY(center.notifications().isEmpty());
}

void NotificationCenterTest::capsNotificationHistory()
{
    NotificationCenter center(nullptr, 2, storePath());
    const QString firstId = center.pushNotification(QStringLiteral("First"), QStringLiteral("One"));
    center.pushNotification(QStringLiteral("Second"), QStringLiteral("Two"));
    const QString thirdId = center.pushNotification(QStringLiteral("Third"), QStringLiteral("Three"));

    QCOMPARE(center.notifications().size(), 2);
    QCOMPARE(center.notifications().first().toMap().value(QStringLiteral("id")).toString(), thirdId);
    QVERIFY(!center.dismissNotification(firstId));
    QVERIFY(center.dismissNotification(thirdId));
}

void NotificationCenterTest::doNotDisturbKeepsNewNotificationsRead()
{
    NotificationCenter center(nullptr, 40, storePath());
    center.setDoNotDisturb(true);
    center.pushNotification(QStringLiteral("Quiet"), QStringLiteral("Stored without a badge"));

    QCOMPARE(center.unreadCount(), 0);
    QVERIFY(center.notifications().first().toMap().value(QStringLiteral("read")).toBool());

    center.setDoNotDisturb(false);
    center.pushNotification(QStringLiteral("Visible"), QStringLiteral("Unread notification"));
    QCOMPARE(center.unreadCount(), 1);
}

void NotificationCenterTest::restoresHistoryAcrossARestart()
{
    QString readId;
    {
        NotificationCenter center(nullptr, 40, storePath());
        readId = center.pushNotification(QStringLiteral("Application started"),
                                         QStringLiteral("Started Terminal"),
                                         QStringLiteral("success"));
        center.pushNotification(QStringLiteral("Application launch failed"),
                                QStringLiteral("Firefox exited immediately"),
                                QStringLiteral("error"));
        QVERIFY(center.markRead(readId));
        QCOMPARE(center.unreadCount(), 1);
    }

    // A new shell on the same account sees the history the old one left.
    NotificationCenter restarted(nullptr, 40, storePath());
    QCOMPARE(restarted.notifications().size(), 2);

    const QVariantMap newest = restarted.notifications().first().toMap();
    QCOMPARE(newest.value(QStringLiteral("title")).toString(),
             QStringLiteral("Application launch failed"));
    QCOMPARE(newest.value(QStringLiteral("kind")).toString(), QStringLiteral("error"));

    // Read and unread state survives too, so the badge is not resurrected.
    QCOMPARE(restarted.unreadCount(), 1);
    const QVariantMap oldest = restarted.notifications().last().toMap();
    QCOMPARE(oldest.value(QStringLiteral("id")).toString(), readId);
    QVERIFY(oldest.value(QStringLiteral("read")).toBool());
}

void NotificationCenterTest::neverReissuesARestoredIdentifier()
{
    QString survivingId;
    {
        NotificationCenter center(nullptr, 40, storePath());
        center.pushNotification(QStringLiteral("First"), QStringLiteral("One"));
        survivingId = center.pushNotification(QStringLiteral("Second"), QStringLiteral("Two"));
    }

    NotificationCenter restarted(nullptr, 40, storePath());
    const QString freshId = restarted.pushNotification(QStringLiteral("Third"),
                                                       QStringLiteral("Three"));

    // Reusing an identifier would make dismissing the new entry silently
    // remove a restored one instead.
    QVERIFY(freshId != survivingId);
    QCOMPARE(restarted.notifications().size(), 3);
    QVERIFY(restarted.dismissNotification(freshId));
    QCOMPARE(restarted.notifications().size(), 2);
    QVERIFY(restarted.dismissNotification(survivingId));
    QCOMPARE(restarted.notifications().size(), 1);
}

void NotificationCenterTest::persistsDismissalAndClearing()
{
    NotificationCenter center(nullptr, 40, storePath());
    const QString firstId = center.pushNotification(QStringLiteral("First"), QStringLiteral("One"));
    center.pushNotification(QStringLiteral("Second"), QStringLiteral("Two"));
    QVERIFY(center.dismissNotification(firstId));

    {
        NotificationCenter restarted(nullptr, 40, storePath());
        QCOMPARE(restarted.notifications().size(), 1);
        QCOMPARE(restarted.notifications().first().toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Second"));
    }

    center.clearNotifications();
    NotificationCenter afterClear(nullptr, 40, storePath());
    QVERIFY(afterClear.notifications().isEmpty());
    QCOMPARE(afterClear.unreadCount(), 0);
}

void NotificationCenterTest::restoresNoMoreThanTheHistoryCap()
{
    {
        NotificationCenter center(nullptr, 40, storePath());
        for (int index = 0; index < 6; ++index) {
            center.pushNotification(QStringLiteral("Event %1").arg(index), QStringLiteral("Body"));
        }
    }

    // A shell configured to hold less than the file contains must not grow
    // back to the larger history.
    NotificationCenter restarted(nullptr, 2, storePath());
    QCOMPARE(restarted.notifications().size(), 2);
    QCOMPARE(restarted.notifications().first().toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("Event 5"));

    restarted.pushNotification(QStringLiteral("Fresh"), QStringLiteral("Body"));
    QCOMPARE(restarted.notifications().size(), 2);
}

void NotificationCenterTest::describesEntryAgeInWords()
{
    const QDateTime now = QDateTime::currentDateTime();

    QCOMPARE(NotificationCenter::relativeTime(now, now), QStringLiteral("Just now"));
    QCOMPARE(NotificationCenter::relativeTime(now.addSecs(-59), now), QStringLiteral("Just now"));
    QCOMPARE(NotificationCenter::relativeTime(now.addSecs(-90), now), QStringLiteral("1m ago"));
    QCOMPARE(NotificationCenter::relativeTime(now.addSecs(-7200), now), QStringLiteral("2h ago"));
    QCOMPARE(NotificationCenter::relativeTime(now.addDays(-1), now), QStringLiteral("Yesterday"));
    QCOMPARE(NotificationCenter::relativeTime(now.addDays(-3), now), QStringLiteral("3d ago"));

    // A clock that moved backwards must not produce a negative age.
    QCOMPARE(NotificationCenter::relativeTime(now.addSecs(3600), now), QStringLiteral("Just now"));

    // Anything past a week falls back to a date rather than a growing count.
    const QString distant = NotificationCenter::relativeTime(now.addDays(-40), now);
    QVERIFY(!distant.isEmpty());
    QVERIFY(!distant.endsWith(QStringLiteral("ago")));

    QVERIFY(NotificationCenter::relativeTime(QDateTime(), now).isEmpty());
}

void NotificationCenterTest::offersADisplayTimeToTheView()
{
    NotificationCenter center(nullptr, 40, storePath());
    center.pushNotification(QStringLiteral("Application started"), QStringLiteral("Started Files"));

    const QVariantMap item = center.notifications().first().toMap();
    QCOMPARE(item.value(QStringLiteral("displayTime")).toString(), QStringLiteral("Just now"));

    // The exact time stays available for the panel's hover text.
    const QString timestamp = item.value(QStringLiteral("timestamp")).toString();
    QVERIFY(QDateTime::fromString(timestamp, Qt::ISODateWithMs).isValid());
}

QTEST_MAIN(NotificationCenterTest)
#include "test-notificationcenter.moc"
