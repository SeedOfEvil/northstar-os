#include "storageaccess.h"
#include <QtTest>

class StorageTest : public QObject {
    Q_OBJECT
private:
    StorageAccess::Objects fixture() {
        return {{QDBusObjectPath("/drive"), {{"org.freedesktop.UDisks2.Drive",
                    {{"ConnectionBus", "usb"}, {"Removable", true}, {"Serial", "test-only"}}}}},
                {QDBusObjectPath("/partition"), {
                    {"org.freedesktop.UDisks2.Block", {{"Drive", QVariant::fromValue(QDBusObjectPath("/drive"))},
                        {"Device", QByteArray("/dev/da0p1\0", 11)}, {"DeviceNumber", 390},
                        {"Size", 1024 * 1024 * 100}, {"IdType", "ntfs"}, {"IdUsage", "filesystem"}}},
                    {"org.freedesktop.UDisks2.Partition", {{"Type", "ms-basic-data"}, {"UUID", "test-uuid"}}},
                    {"org.freedesktop.UDisks2.Filesystem", {}}}}};
    }
    bool eligible(const StorageAccess::Objects &objects) {
        const auto rows = StorageAccess::describe(objects);
        return !rows.isEmpty() && rows.first().toMap().value("eligible").toBool();
    }
private slots:
    void usbNtfs() { QVERIFY(eligible(fixture())); }
    void internalDenied() {
        auto objects = fixture();
        objects[QDBusObjectPath("/drive")]["org.freedesktop.UDisks2.Drive"]["ConnectionBus"] = "pci";
        QVERIFY(StorageAccess::describe(objects).isEmpty());
    }
    void protectedDenied() {
        for (const QString &field : {QStringLiteral("HintSystem"), QStringLiteral("HintIgnore")}) {
            auto objects = fixture();
            objects[QDBusObjectPath("/partition")]["org.freedesktop.UDisks2.Block"][field] = true;
            QVERIFY(!eligible(objects));
        }
    }
    void helperDenied() {
        auto objects = fixture();
        objects[QDBusObjectPath("/partition")]["org.freedesktop.UDisks2.Block"]["IdLabel"] = "UEFI:NTFS";
        QVERIFY(!eligible(objects));
    }
    void systemSiblingDenied() {
        auto objects = fixture();
        objects[QDBusObjectPath("/sibling")] = objects.value(QDBusObjectPath("/partition"));
        objects[QDBusObjectPath("/sibling")]["org.freedesktop.UDisks2.Partition"]["Type"] = "freebsd-zfs";
        QVERIFY(!eligible(objects));
    }
    void identityChanges() {
        auto objects = fixture();
        const auto before = StorageAccess::describe(objects).first().toMap().value("identity");
        objects[QDBusObjectPath("/drive")]["org.freedesktop.UDisks2.Drive"]["Serial"] = "replacement";
        QVERIFY(before != StorageAccess::describe(objects).first().toMap().value("identity"));
        objects[QDBusObjectPath("/drive")]["org.freedesktop.UDisks2.Drive"]["Serial"] = "";
        QVERIFY(!eligible(objects));
    }
    void paths() {
        QCOMPARE(StorageAccess::mountPath(1002, "/dev/da0p1"), "/media/northstar/1002-da0p1");
        QVERIFY(StorageAccess::mountPath(1002, "/dev/../nda0p1").isEmpty());
        QVERIFY(StorageAccess::mountPath(1002, "/dev/nda0p1").isEmpty());
    }
    void safeRemovalMounts() {
        QVERIFY(!StorageAccess::diskHasMounts("/dev/da0p1", {"/dev/da1p1", "nstar/home"}));
        QVERIFY(StorageAccess::diskHasMounts("/dev/da0p1", {"/dev/da0p2"}));
        QVERIFY(StorageAccess::diskHasMounts("/dev/da0p1", {"/dev/da0"}));
        QVERIFY(!StorageAccess::diskHasMounts("/dev/da0p1", {"/dev/da01p1"}));
        QVERIFY(StorageAccess::diskHasMounts("/dev/da0p1", {"/dev/gpt/unknown"}));
        QVERIFY(StorageAccess::diskHasMounts("invalid", {}));
    }
    void safeRemovalAliases() {
        const QByteArray labels = "Name Status Components\nmsdosfs/NSTAR_EFI N/A nda0p1\ngpt/USB N/A da0p2\n";
        const auto internal = StorageAccess::resolveMountSources({"/dev/msdosfs/NSTAR_EFI"}, labels);
        QCOMPARE(internal, QStringList{"/dev/nda0p1"});
        QVERIFY(!StorageAccess::diskHasMounts("/dev/da0p1", internal));
        QVERIFY(StorageAccess::diskHasMounts("/dev/da0p1",
            StorageAccess::resolveMountSources({"/dev/gpt/USB"}, labels)));
        QVERIFY(StorageAccess::diskHasMounts("/dev/da0p1",
            StorageAccess::resolveMountSources({"/dev/gpt/unknown"}, labels)));
        QCOMPARE(StorageAccess::resolveMountSources({"/dev/a"}, "a N/A b\nb N/A a\n"), QStringList{"/dev/a"});
    }
    void unsupportedFilesystem() {
        auto objects = fixture();
        objects[QDBusObjectPath("/partition")]["org.freedesktop.UDisks2.Block"]["IdType"] = "vfat";
        QVERIFY(!eligible(objects));
    }
    void smallPartition() {
        auto objects = fixture();
        objects[QDBusObjectPath("/partition")]["org.freedesktop.UDisks2.Block"]["Size"] = 1024;
        QVERIFY(!eligible(objects));
    }
    void liveInventory() {
        if (!qEnvironmentVariableIsSet("NORTHSTAR_TEST_STORAGE_LIVE"))
            QSKIP("Opt-in read-only storage service inspection");
        const auto snapshot = StorageAccess::inspect();
        QVERIFY2(snapshot.error.isEmpty(), qPrintable(snapshot.error));
        for (const auto &entry : StorageAccess::describe(snapshot.objects)) {
            const auto row = entry.toMap();
            qInfo().noquote() << row.value("device").toString()
                << "eligible=" << row.value("eligible").toBool() << row.value("reason").toString();
        }
    }
};
QTEST_GUILESS_MAIN(StorageTest)
#include "test-storageaccess.moc"
