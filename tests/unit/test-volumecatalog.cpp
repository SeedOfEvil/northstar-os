#include "volumecatalog.h"

#include <QVariantMap>
#include <QtTest/QtTest>

class VolumeCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void discoversSystemVolume();
    void removableMetadata();
    void liveInventoryIsNonActionable();
    void automaticallyPublishesInventory();
};

void VolumeCatalogTest::automaticallyPublishesInventory()
{
    VolumeController controller;
    QSignalSpy finished(&controller, &VolumeController::removableScanFinished);
    bool completionWasPublished = false;
    connect(&controller, &VolumeController::removableScanFinished, this, [&] {
        completionWasPublished = !controller.scanning() && !controller.removableStatus().isEmpty();
    });
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() > 0, 20000);
    QVERIFY(completionWasPublished);
    controller.scanRemovable();
    QVERIFY(controller.scanning());
    controller.scanRemovable(); // Coalesce overlapping requests.
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 20000);
    QVERIFY(!controller.scanning());
}

void VolumeCatalogTest::liveInventoryIsNonActionable()
{
    const auto result = RemovableStorage::scan();
    QVERIFY(!result.status.isEmpty());
    qInfo().noquote() << result.status;
    for (const auto &value : result.devices) {
        const auto device = value.toMap();
        QVERIFY(!device.value("canMount").toBool());
        QVERIFY(!device.value("canEject").toBool());
        qInfo().noquote() << device.value("device").toString() << device.value("name").toString();
    }
}

void VolumeCatalogTest::removableMetadata()
{
    const QByteArray xml = "<mesh><class><name>DISK</name><geom><provider>"
        "<name>da0</name><mediasize>61872793600</mediasize><config>"
        "<descr>Kingston &amp; Test</descr><ident>PRIVATE-SERIAL</ident></config>"
        "</provider></geom><geom><provider><name>nda0</name><mediasize>256000000000</mediasize>"
        "</provider></geom></class><class><name>PART</name><geom><provider>"
        "<name>da0</name><mediasize>1</mediasize></provider></geom></class></mesh>";
    bool valid;
    const auto devices = RemovableStorage::parse(xml, {"da0", "nda0"}, &valid);
    QVERIFY(valid);
    QCOMPARE(devices.size(), 1);
    const auto device = devices.first().toMap();
    QCOMPARE(device.value("name").toString(), QString("Kingston & Test"));
    QCOMPARE(device.value("totalBytes").toLongLong(), 61872793600LL);
    QVERIFY(!device.value("canMount").toBool());
    QVERIFY(!device.value("canEject").toBool());
    QVERIFY(!device.contains("ident"));
    QCOMPARE(RemovableStorage::parse(xml, {}, &valid).size(), 0);
    QVERIFY(valid);
    QVERIFY(RemovableStorage::parse(xml.left(xml.size() - 8), {"da0"}, &valid).isEmpty());
    QVERIFY(!valid);
    QVERIFY(RemovableStorage::parse(QByteArray(2 * 1024 * 1024 + 1, 'x'), {"da0"}, &valid).isEmpty());
    QVERIFY(!valid);
    QVERIFY(RemovableStorage::parse("<!DOCTYPE mesh [<!ENTITY x 'bad'>]><mesh/>", {}, &valid).isEmpty());
    QVERIFY(!valid);
}

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
