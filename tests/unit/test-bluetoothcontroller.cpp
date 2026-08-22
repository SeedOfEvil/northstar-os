#include "bluetoothcontroller.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class BluetoothControllerTest final : public QObject
{
    Q_OBJECT
private slots:
    void scansAndSortsDeviceState();
    void pairsWithoutWritingThePinToTheRequest();
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
        "'device=aabbccddeeff|4d6f757365|0|0' "
        "'device=112233445566|50686f6e65|1|1'\n");
    qputenv("NORTHSTAR_BLUETOOTH_SCAN_COMMAND", scanner.toUtf8());
    BluetoothController controller;
    QVERIFY(controller.refreshDevices());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.devices().size(), 2);
    const QVariantMap first = controller.devices().first().toMap();
    QCOMPARE(first.value(QStringLiteral("name")).toString(), QStringLiteral("Phone"));
    QVERIFY(first.value(QStringLiteral("remembered")).toBool());
    QVERIFY(first.value(QStringLiteral("connected")).toBool());
    QCOMPARE(controller.statusMessage(), QStringLiteral("Connected to Phone."));
    QVERIFY(!controller.statusIsError());
    qunsetenv("NORTHSTAR_BLUETOOTH_SCAN_COMMAND");
}

void BluetoothControllerTest::pairsWithoutWritingThePinToTheRequest()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString events = directory.filePath(QStringLiteral("events"));
    const QString helper = writeExecutable(directory, QStringLiteral("auth-helper"),
        "#!/bin/sh\n"
        "printf '%s\\n' NORTHSTAR_BLUETOOTH_AUTHORIZED=1\n"
        "grep -Eq 'pin|password|secret|key' \"$2\" && exit 65\n"
        "IFS= read -r pin\n"
        "[ \"$pin\" = 654321 ] || exit 65\n"
        "grep -Fx 'address_hex=aabbccddeeff' \"$2\" >/dev/null || exit 65\n"
        "printf '%s\\n' paired > \"$NORTHSTAR_BLUETOOTH_TEST_EVENTS\"\n");
    qputenv("NORTHSTAR_BLUETOOTH_AUTH_COMMAND", helper.toUtf8());
    qputenv("NORTHSTAR_BLUETOOTH_TEST_EVENTS", events.toUtf8());
    BluetoothController controller;
    QSignalSpy cleared(&controller, &BluetoothController::secretsCleared);
    QSignalSpy expected(&controller, &BluetoothController::authorizationPromptExpected);
    QSignalSpy completed(&controller, &BluetoothController::authorizationCompleted);
    QSignalSpy finished(&controller, &BluetoothController::pairingFinished);
    QVERIFY(!controller.pairDevice(QStringLiteral("aabbccddeeff"),
                                   QStringLiteral("Phone"),
                                   QStringLiteral("12")));
    QVERIFY(controller.pairDevice(QStringLiteral("aabbccddeeff"),
                                  QStringLiteral("Phone"),
                                  QStringLiteral("654321")));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QVERIFY(finished.first().first().toBool());
    QVERIFY(QFile::exists(events));
    QVERIFY(cleared.size() >= 1);
    QCOMPARE(expected.size(), 1);
    QCOMPARE(completed.size(), 1);
    QVERIFY(!controller.statusIsError());
    qunsetenv("NORTHSTAR_BLUETOOTH_AUTH_COMMAND");
    qunsetenv("NORTHSTAR_BLUETOOTH_TEST_EVENTS");
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
    QVERIFY(controller.pairDevice(QStringLiteral("aabbccddeeff"),
                                  QStringLiteral("Phone"),
                                  QStringLiteral("654321")));
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
