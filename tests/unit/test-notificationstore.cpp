#include "notificationstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class NotificationStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void writesAUserPrivateHistoryFile();
    void roundTripsEveryField();
    void honoursTheRequestedLimit();
    void dropsEntriesPastTheRetentionWindow();
    void skipsMalformedRecords();
    void normalisesAHandEditedKind();
    void shrinksWhenTheHistoryShrinks();
    void readsNothingFromAnAbsentFile();

private:
    QString storePath() const;
    static NotificationEntry entry(const QString &id, const QDateTime &when);

    QTemporaryDir *m_directory = nullptr;
};

void NotificationStoreTest::init()
{
    m_directory = new QTemporaryDir;
    QVERIFY(m_directory->isValid());
}

void NotificationStoreTest::cleanup()
{
    delete m_directory;
    m_directory = nullptr;
}

QString NotificationStoreTest::storePath() const
{
    return m_directory->filePath(QStringLiteral("notifications.ini"));
}

NotificationEntry NotificationStoreTest::entry(const QString &id, const QDateTime &when)
{
    NotificationEntry created;
    created.id = id;
    created.title = QStringLiteral("Title %1").arg(id);
    created.body = QStringLiteral("Body %1").arg(id);
    created.kind = QStringLiteral("info");
    created.timestamp = when.toString(Qt::ISODateWithMs);
    return created;
}

void NotificationStoreTest::writesAUserPrivateHistoryFile()
{
    const NotificationStore store(storePath());
    QCOMPARE(store.settingsPath(), storePath());
    QVERIFY(store.save({entry(QStringLiteral("notification-1"), QDateTime::currentDateTime())}));
    QVERIFY(QFile::exists(storePath()));

    // The history names the applications this account runs.
    const QFile::Permissions permissions = QFile::permissions(storePath());
    QVERIFY(!permissions.testFlag(QFile::ReadGroup));
    QVERIFY(!permissions.testFlag(QFile::WriteGroup));
    QVERIFY(!permissions.testFlag(QFile::ReadOther));
    QVERIFY(!permissions.testFlag(QFile::WriteOther));
}

void NotificationStoreTest::roundTripsEveryField()
{
    const QDateTime when = QDateTime::currentDateTime().addSecs(-90);

    NotificationEntry written;
    written.id = QStringLiteral("notification-7");
    written.title = QStringLiteral("Application launch failed");
    written.body = QStringLiteral("Firefox exited before it drew a window");
    written.kind = QStringLiteral("error");
    written.timestamp = when.toString(Qt::ISODateWithMs);
    written.read = true;

    const NotificationStore store(storePath());
    QVERIFY(store.save({written}));

    const QList<NotificationEntry> restored = store.load(40);
    QCOMPARE(restored.size(), 1);
    QCOMPARE(restored.first(), written);
}

void NotificationStoreTest::honoursTheRequestedLimit()
{
    const QDateTime now = QDateTime::currentDateTime();
    QList<NotificationEntry> written;
    for (int index = 1; index <= 6; ++index) {
        written.append(entry(QStringLiteral("notification-%1").arg(index), now.addSecs(-index)));
    }

    const NotificationStore store(storePath());
    QVERIFY(store.save(written));

    const QList<NotificationEntry> restored = store.load(3);
    QCOMPARE(restored.size(), 3);

    // The file is written newest first, so a limit keeps the newest entries.
    QCOMPARE(restored.first().id, QStringLiteral("notification-1"));
    QCOMPARE(restored.last().id, QStringLiteral("notification-3"));
}

void NotificationStoreTest::dropsEntriesPastTheRetentionWindow()
{
    const QDateTime now = QDateTime::currentDateTime();
    const int retention = NotificationStore::retentionDays();
    QVERIFY(retention > 0);

    const QList<NotificationEntry> written{
        entry(QStringLiteral("notification-1"), now.addSecs(-60)),
        entry(QStringLiteral("notification-2"), now.addDays(-(retention - 1))),
        entry(QStringLiteral("notification-3"), now.addDays(-(retention + 1))),
        // A file carried across a clock change can be stamped far ahead.
        entry(QStringLiteral("notification-4"), now.addDays(30)),
    };

    const NotificationStore store(storePath());
    QVERIFY(store.save(written));

    const QList<NotificationEntry> restored = store.load(40);
    QCOMPARE(restored.size(), 2);
    QCOMPARE(restored.at(0).id, QStringLiteral("notification-1"));
    QCOMPARE(restored.at(1).id, QStringLiteral("notification-2"));
}

void NotificationStoreTest::skipsMalformedRecords()
{
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);

    // A hand-edited or truncated file must never stop the shell from starting.
    {
        QSettings settings(storePath(), QSettings::IniFormat);
        settings.beginWriteArray(QStringLiteral("notifications"), 4);

        settings.setArrayIndex(0);
        settings.setValue(QStringLiteral("id"), QString());
        settings.setValue(QStringLiteral("title"), QStringLiteral("No identity"));
        settings.setValue(QStringLiteral("timestamp"), timestamp);

        settings.setArrayIndex(1);
        settings.setValue(QStringLiteral("id"), QStringLiteral("notification-2"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Unreadable time"));
        settings.setValue(QStringLiteral("timestamp"), QStringLiteral("last tuesday"));

        settings.setArrayIndex(2);
        settings.setValue(QStringLiteral("id"), QStringLiteral("notification-3"));
        settings.setValue(QStringLiteral("title"), QString());
        settings.setValue(QStringLiteral("timestamp"), timestamp);

        settings.setArrayIndex(3);
        settings.setValue(QStringLiteral("id"), QStringLiteral("notification-4"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Intact"));
        settings.setValue(QStringLiteral("body"), QStringLiteral("The only usable record"));
        settings.setValue(QStringLiteral("kind"), QStringLiteral("warning"));
        settings.setValue(QStringLiteral("timestamp"), timestamp);
        settings.endArray();
    }

    const QList<NotificationEntry> restored = NotificationStore(storePath()).load(40);
    QCOMPARE(restored.size(), 1);
    QCOMPARE(restored.first().id, QStringLiteral("notification-4"));
    QCOMPARE(restored.first().kind, QStringLiteral("warning"));
}

void NotificationStoreTest::normalisesAHandEditedKind()
{
    NotificationEntry written = entry(QStringLiteral("notification-1"), QDateTime::currentDateTime());
    written.kind = QStringLiteral("CATASTROPHE");
    QVERIFY(NotificationStore(storePath()).save({written}));

    const QList<NotificationEntry> restored = NotificationStore(storePath()).load(40);
    QCOMPARE(restored.size(), 1);
    QCOMPARE(restored.first().kind, QStringLiteral("info"));
}

void NotificationStoreTest::shrinksWhenTheHistoryShrinks()
{
    const QDateTime now = QDateTime::currentDateTime();
    const NotificationStore store(storePath());
    QVERIFY(store.save({entry(QStringLiteral("notification-1"), now),
                        entry(QStringLiteral("notification-2"), now),
                        entry(QStringLiteral("notification-3"), now)}));
    QCOMPARE(store.load(40).size(), 3);

    // Dismissing entries has to shorten the file, not just overwrite its head.
    QVERIFY(store.save({entry(QStringLiteral("notification-9"), now)}));
    const QList<NotificationEntry> restored = store.load(40);
    QCOMPARE(restored.size(), 1);
    QCOMPARE(restored.first().id, QStringLiteral("notification-9"));

    QVERIFY(store.save({}));
    QVERIFY(store.load(40).isEmpty());
}

void NotificationStoreTest::readsNothingFromAnAbsentFile()
{
    const NotificationStore store(m_directory->filePath(QStringLiteral("absent/history.ini")));
    QVERIFY(store.load(40).isEmpty());

    // The default path stays inside the configuration this account owns.
    QVERIFY(NotificationStore::defaultSettingsPath().endsWith(QStringLiteral("notifications.ini")));
    QVERIFY(!NotificationStore().settingsPath().isEmpty());
}

QTEST_MAIN(NotificationStoreTest)
#include "test-notificationstore.moc"
