#include "bluetoothcontroller.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

class BluetoothControllerTest final : public QObject
{
    Q_OBJECT
private slots:
    void scansAndSortsDeviceState();
    void confirmsSecureSimplePairingWithoutWritingSecrets();
    void forgetsAndChangesDiscoverability();
    void sendsAndReceivesFilesWithoutPrivilege();
    void restoresWindowLifecycleWhenAuthorizationIsCancelled();
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

void BluetoothControllerTest::scansAndSortsDeviceState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString scanner = writeExecutable(directory, QStringLiteral("scanner"),
        "#!/bin/sh\nprintf '%s\\n' NORTHSTAR_BLUETOOTH_SCAN=1 "
        "'discoverable=1' "
        "'device=aabbccddeeff|4d6f757365|0|0|0' "
        "'device=112233445566|50686f6e65|1|1|1'\n");
    qputenv("NORTHSTAR_BLUETOOTH_SCAN_COMMAND", scanner.toUtf8());
    BluetoothController controller;
    QVERIFY(controller.refreshDevices());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.devices().size(), 2);
    const QVariantMap first = controller.devices().first().toMap();
    QCOMPARE(first.value(QStringLiteral("name")).toString(), QStringLiteral("Phone"));
    QVERIFY(first.value(QStringLiteral("remembered")).toBool());
    QVERIFY(first.value(QStringLiteral("paired")).toBool());
    QVERIFY(first.value(QStringLiteral("connected")).toBool());
    QVERIFY(controller.discoverable());
    QCOMPARE(controller.statusMessage(), QStringLiteral("Connected to Phone."));
    QVERIFY(!controller.statusIsError());
    qunsetenv("NORTHSTAR_BLUETOOTH_SCAN_COMMAND");
}

void BluetoothControllerTest::confirmsSecureSimplePairingWithoutWritingSecrets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString events = directory.filePath(QStringLiteral("events"));
    const QString helper = writeExecutable(directory, QStringLiteral("auth-helper"),
        "#!/bin/sh\n"
        "printf '%s\\n' NORTHSTAR_BLUETOOTH_AUTHORIZED=1\n"
        "[ \"$1\" = --pair ] || exit 64\n"
        "grep -Eq 'pin|password|secret|key' \"$2\" && exit 65\n"
        "grep -Fx 'address_hex=aabbccddeeff' \"$2\" >/dev/null || exit 65\n"
        "printf '%s\\n' NORTHSTAR_BLUETOOTH_INBOUND_PAIRING=WAITING\n"
        "printf '%s\\n' NORTHSTAR_BLUETOOTH_CONFIRM=654321\n"
        "IFS= read -r decision\n"
        "[ \"$decision\" = accept ] || exit 125\n"
        "printf '%s\\n' paired > \"$NORTHSTAR_BLUETOOTH_TEST_EVENTS\"\n");
    qputenv("NORTHSTAR_BLUETOOTH_AUTH_COMMAND", helper.toUtf8());
    qputenv("NORTHSTAR_BLUETOOTH_TEST_EVENTS", events.toUtf8());
    BluetoothController controller;
    QSignalSpy expected(&controller, &BluetoothController::authorizationPromptExpected);
    QSignalSpy completed(&controller, &BluetoothController::authorizationCompleted);
    QSignalSpy confirmation(&controller, &BluetoothController::pairingConfirmationRequested);
    QSignalSpy finished(&controller, &BluetoothController::pairingFinished);
    QVERIFY(!controller.pairDevice(QStringLiteral("invalid"), QStringLiteral("Phone")));
    QVERIFY(controller.pairDevice(QStringLiteral("aabbccddeeff"), QStringLiteral("Phone")));
    QTRY_COMPARE_WITH_TIMEOUT(confirmation.size(), 1, 3000);
    QVERIFY(controller.awaitingConfirmation());
    QCOMPARE(controller.confirmationCode(), QStringLiteral("654321"));
    QVERIFY(controller.respondToPairing(true));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QVERIFY(finished.first().first().toBool());
    QVERIFY(QFile::exists(events));
    QCOMPARE(expected.size(), 1);
    QCOMPARE(completed.size(), 1);
    QVERIFY(!controller.statusIsError());
    qunsetenv("NORTHSTAR_BLUETOOTH_AUTH_COMMAND");
    qunsetenv("NORTHSTAR_BLUETOOTH_TEST_EVENTS");
}

void BluetoothControllerTest::forgetsAndChangesDiscoverability()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString events = directory.filePath(QStringLiteral("events"));
    const QString helper = writeExecutable(directory, QStringLiteral("auth-helper"),
        "#!/bin/sh\n"
        "printf '%s\\n' NORTHSTAR_BLUETOOTH_AUTHORIZED=1\n"
        "case \"$1\" in\n"
        "  --forget) grep -Fx 'address_hex=aabbccddeeff' \"$2\" >/dev/null || exit 65; "
        "printf '%s\\n' forgot >> \"$NORTHSTAR_BLUETOOTH_TEST_EVENTS\";;\n"
        "  --discoverable) [ \"$2\" = on ] || exit 65; "
        "printf '%s\\n' visible >> \"$NORTHSTAR_BLUETOOTH_TEST_EVENTS\";;\n"
        "  *) exit 64;;\n"
        "esac\n");
    qputenv("NORTHSTAR_BLUETOOTH_AUTH_COMMAND", helper.toUtf8());
    qputenv("NORTHSTAR_BLUETOOTH_TEST_EVENTS", events.toUtf8());
    BluetoothController controller;
    QSignalSpy forgotten(&controller, &BluetoothController::forgetFinished);
    QVERIFY(controller.forgetDevice(QStringLiteral("aabbccddeeff")));
    QTRY_COMPARE_WITH_TIMEOUT(forgotten.size(), 1, 3000);
    QVERIFY(forgotten.first().first().toBool());
    QVERIFY(controller.setDiscoverable(true));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QVERIFY(controller.discoverable());
    QFile eventFile(events);
    QVERIFY(eventFile.open(QIODevice::ReadOnly));
    QCOMPARE(eventFile.readAll(), QByteArray("forgot\nvisible\n"));
    qunsetenv("NORTHSTAR_BLUETOOTH_AUTH_COMMAND");
    qunsetenv("NORTHSTAR_BLUETOOTH_TEST_EVENTS");
}

void BluetoothControllerTest::sendsAndReceivesFilesWithoutPrivilege()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString events = directory.filePath(QStringLiteral("events"));
    const QString obex = writeExecutable(directory, QStringLiteral("obexapp"),
        "#!/bin/sh\n"
        "if [ \"$1\" = --start ]; then\n"
        "  printf '%s\\n' NORTHSTAR_BLUETOOTH_AUTHORIZED=1\n"
        "  printf 'server=authorized\\n' >> \"$NORTHSTAR_BLUETOOTH_TEST_EVENTS\"\n"
        "  printf '%s\\n' $$ > \"$NORTHSTAR_BLUETOOTH_TEST_SERVER_PID\"\n"
        "  trap 'exit 0' TERM INT; while :; do sleep 1; done\n"
        "elif [ \"$1\" = --stop ]; then\n"
        "  printf 'server=stopped\\n' >> \"$NORTHSTAR_BLUETOOTH_TEST_EVENTS\"\n"
        "  kill -TERM \"$(cat \"$NORTHSTAR_BLUETOOTH_TEST_SERVER_PID\")\"\n"
        "  exit 0\n"
        "fi\n"
        "case \"$1\" in\n"
        "  -c) printf 'client=%s\\n' \"$*\" >> \"$NORTHSTAR_BLUETOOTH_TEST_EVENTS\";;\n"
        "  *) exit 64;;\n"
        "esac\n");
    const QString payload = directory.filePath(QStringLiteral("payload.txt"));
    QFile payloadFile(payload);
    QVERIFY(payloadFile.open(QIODevice::WriteOnly));
    QCOMPARE(payloadFile.write("northstar\n"), qint64(10));
    payloadFile.close();
    qputenv("NORTHSTAR_BLUETOOTH_OBEX_COMMAND", obex.toUtf8());
    qputenv("NORTHSTAR_BLUETOOTH_OBEX_RECEIVE_COMMAND", obex.toUtf8());
    qputenv("NORTHSTAR_BLUETOOTH_TEST_EVENTS", events.toUtf8());
    qputenv("NORTHSTAR_BLUETOOTH_TEST_SERVER_PID",
            directory.filePath(QStringLiteral("server.pid")).toUtf8());
    BluetoothController controller;
    QVERIFY(controller.fileTransferAvailable());
    QVERIFY(controller.setReceivingFiles(true));
    QTRY_VERIFY_WITH_TIMEOUT(controller.receivingFiles(), 3000);
    QVERIFY(controller.setReceivingFiles(false));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.receivingFiles(), 3000);
    QVERIFY(controller.sendFile(QStringLiteral("aabbccddeeff"),
                                QUrl::fromLocalFile(payload).toString()));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QFile eventFile(events);
    QVERIFY(eventFile.open(QIODevice::ReadOnly));
    const QByteArray output = eventFile.readAll();
    QVERIFY(output.contains("server=authorized"));
    QVERIFY(output.contains("server=stopped"));
    QVERIFY(output.contains("client=-c -a aa:bb:cc:dd:ee:ff -C OPUSH -n put "));
    QVERIFY(output.contains("payload.txt"));
    QCOMPARE(controller.statusMessage(), QStringLiteral("The file was sent over Bluetooth."));
    QVERIFY(!controller.statusIsError());
    qunsetenv("NORTHSTAR_BLUETOOTH_OBEX_COMMAND");
    qunsetenv("NORTHSTAR_BLUETOOTH_OBEX_RECEIVE_COMMAND");
    qunsetenv("NORTHSTAR_BLUETOOTH_TEST_EVENTS");
    qunsetenv("NORTHSTAR_BLUETOOTH_TEST_SERVER_PID");
}

void BluetoothControllerTest::restoresWindowLifecycleWhenAuthorizationIsCancelled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString helper = writeExecutable(directory, QStringLiteral("cancel-helper"),
                                            "#!/bin/sh\nexit 126\n");
    qputenv("NORTHSTAR_BLUETOOTH_AUTH_COMMAND", helper.toUtf8());
    BluetoothController controller;
    QSignalSpy expected(&controller, &BluetoothController::authorizationPromptExpected);
    QSignalSpy completed(&controller, &BluetoothController::authorizationCompleted);
    QVERIFY(controller.pairDevice(QStringLiteral("aabbccddeeff"), QStringLiteral("Phone")));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(expected.size(), 1);
    QCOMPARE(completed.size(), 1);
    QCOMPARE(controller.statusMessage(),
             QStringLiteral("Administrator authorization was cancelled."));
    QVERIFY(controller.statusIsError());
    qunsetenv("NORTHSTAR_BLUETOOTH_AUTH_COMMAND");
}

QTEST_MAIN(BluetoothControllerTest)
#include "test-bluetoothcontroller.moc"
