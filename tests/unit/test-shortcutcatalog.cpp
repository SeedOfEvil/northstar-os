#include "shortcutcatalog.h"

#include <QVariantMap>
#include <QtTest/QtTest>

class ShortcutCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesStableApplicationMappings();
    void rejectsUnknownCommands();
};

void ShortcutCatalogTest::exposesStableApplicationMappings()
{
    ShortcutCatalog catalog;

    QCOMPARE(catalog.sequenceFor(QStringLiteral("applications")), QStringLiteral("Meta+K"));
    QCOMPARE(catalog.sequenceFor(QStringLiteral("files")), QStringLiteral("Meta+E"));
    QCOMPARE(catalog.sequenceFor(QStringLiteral("settings")), QStringLiteral("Meta+,"));
    QCOMPARE(catalog.sequenceFor(QStringLiteral("terminal")), QStringLiteral("Ctrl+Alt+T"));
    QCOMPARE(catalog.sequenceFor(QStringLiteral("browser")), QStringLiteral("Meta+B"));
    QCOMPARE(catalog.sequenceFor(QStringLiteral("refresh")), QStringLiteral("Meta+R"));

    const QVariantList entries = catalog.shortcuts();
    QCOMPARE(entries.size(), 6);
    QCOMPARE(entries.first().toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Applications"));
}

void ShortcutCatalogTest::rejectsUnknownCommands()
{
    ShortcutCatalog catalog;
    QVERIFY(catalog.sequenceFor(QStringLiteral("not-a-command")).isEmpty());
}

QTEST_MAIN(ShortcutCatalogTest)
#include "test-shortcutcatalog.moc"
