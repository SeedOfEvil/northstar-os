#include "volumecatalog.h"

#include <QVariantMap>
#include <QtTest/QtTest>

class VolumeCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void discoversSystemVolume();
};

void VolumeCatalogTest::discoversSystemVolume()
{
    VolumeController controller;
    const QVariantList volumes = controller.volumes();
    QVERIFY(!volumes.isEmpty());

    bool hasSystemVolume = false;
    for (const QVariant &volume : volumes) {
        const QVariantMap item = volume.toMap();
        QVERIFY(!item.value(QStringLiteral("path")).toString().isEmpty());
        QVERIFY(!item.value(QStringLiteral("name")).toString().isEmpty());
        if (item.value(QStringLiteral("isSystem")).toBool()) {
            hasSystemVolume = true;
            QCOMPARE(item.value(QStringLiteral("path")).toString(), QStringLiteral("/"));
        }
    }
    QVERIFY(hasSystemVolume);
}

QTEST_MAIN(VolumeCatalogTest)
#include "test-volumecatalog.moc"
