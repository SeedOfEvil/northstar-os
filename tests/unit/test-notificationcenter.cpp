#include "notificationcenter.h"

#include <QVariantMap>
#include <QtTest/QtTest>

class NotificationCenterTest final : public QObject
{
    Q_OBJECT

private slots:
    void pushesNewestUnreadNotification();
    void marksAndDismissesNotifications();
    void capsNotificationHistory();
};

void NotificationCenterTest::pushesNewestUnreadNotification()
{
    NotificationCenter center;

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
    NotificationCenter center;
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
    NotificationCenter center(nullptr, 2);
    const QString firstId = center.pushNotification(QStringLiteral("First"), QStringLiteral("One"));
    center.pushNotification(QStringLiteral("Second"), QStringLiteral("Two"));
    const QString thirdId = center.pushNotification(QStringLiteral("Third"), QStringLiteral("Three"));

    QCOMPARE(center.notifications().size(), 2);
    QCOMPARE(center.notifications().first().toMap().value(QStringLiteral("id")).toString(), thirdId);
    QVERIFY(!center.dismissNotification(firstId));
    QVERIFY(center.dismissNotification(thirdId));
}

QTEST_MAIN(NotificationCenterTest)
#include "test-notificationcenter.moc"
