#include "bluetoothcontroller.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class BluetoothControllerTest final : public QObject
{
    Q_OBJECT
private slots:
    void scansAndSortsDevices();
    void launchesValidatedSetupCommand();
};

static QString writeExecutable(QTemporaryDir &directory, const QString &name, const QByteArray &body)
{
    const QString path = directory.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write(body);
    file.close();
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

void BluetoothControllerTest::scansAndSortsDevices()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString scanner = writeExecutable(directory, QStringLiteral("scanner"),
        "#!/bin/sh\nprintf '%s\n' NORTHSTAR_BLUETOOTH_SCAN=1 "
        "'device=aabbccddeeff|4d6f757365' "
        "'device=112233445566|4b6579626f617264'\n");
    qputenv("NORTHSTAR_BLUETOOTH_SCAN_COMMAND", scanner.toUtf8());
    BluetoothController controller;
    QVERIFY(controller.refreshDevices());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.devices().size(), 2);
    QCOMPARE(controller.devices().first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Keyboard"));
    QVERIFY(!controller.statusIsError());
    qunsetenv("NORTHSTAR_BLUETOOTH_SCAN_COMMAND");
}

void BluetoothControllerTest::launchesValidatedSetupCommand()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString events = directory.filePath(QStringLiteral("events"));
    const QString setup = writeExecutable(directory, QStringLiteral("setup"),
        "#!/bin/sh\nprintf '%s\\n' \"$1\" > \"$NORTHSTAR_BLUETOOTH_TEST_EVENTS\"\n");
    qputenv("NORTHSTAR_BLUETOOTH_SETUP_COMMAND", setup.toUtf8());
    qputenv("NORTHSTAR_BLUETOOTH_TEST_EVENTS", events.toUtf8());
    BluetoothController controller;
    QSignalSpy launched(&controller, &BluetoothController::setupWizardLaunched);
    QVERIFY(!controller.openSetupWizard(QStringLiteral("not-an-address")));
    QVERIFY(controller.openSetupWizard(QStringLiteral("aabbccddeeff")));
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(events), 3000);
    QCOMPARE(launched.size(), 1);
    QFile eventFile(events);
    QVERIFY(eventFile.open(QIODevice::ReadOnly));
    QCOMPARE(eventFile.readAll().trimmed(), QByteArray("aabbccddeeff"));
    qunsetenv("NORTHSTAR_BLUETOOTH_SETUP_COMMAND");
    qunsetenv("NORTHSTAR_BLUETOOTH_TEST_EVENTS");
}

QTEST_MAIN(BluetoothControllerTest)
#include "test-bluetoothcontroller.moc"
