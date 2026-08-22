#include "wificontroller.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class WifiControllerTest final : public QObject
{
    Q_OBJECT
private slots:
    void scansAndSortsNetworks();
    void connectsWithoutWritingTheSecretToTheRequest();
};

static QString writeHelper(QTemporaryDir &directory)
{
    const QString path = directory.filePath(QStringLiteral("auth-helper"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write(
        "#!/bin/sh\n"
        "case \"$1\" in\n"
        "  --scan) printf '%s\\n' NORTHSTAR_WIFI_SCAN=1 "
        "'network=43616665|open|-75' "
        "'network=54657374204e6574|secured|-60' "
        "'network=54657374204e6574|secured|-42';;\n"
        "  --connect) "
        "if grep -Eq 'passphrase|password|psk' \"$2\"; then exit 65; fi; "
        "IFS= read -r secret; [ \"$secret\" = 'correct horse' ] || exit 65; "
        "printf '%s\\n' NORTHSTAR_WIFI_CONNECT=PASS;;\n"
        "esac\n");
    file.close();
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

void WifiControllerTest::scansAndSortsNetworks()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("NORTHSTAR_WIFI_AUTH_COMMAND", writeHelper(directory).toUtf8());
    WifiController controller;
    QVERIFY(controller.refreshNetworks());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3000);
    QCOMPARE(controller.networks().size(), 2);
    QCOMPARE(controller.networks().first().toMap().value(QStringLiteral("ssid")).toString(),
             QStringLiteral("Test Net"));
    QCOMPARE(controller.networks().first().toMap().value(QStringLiteral("signal")).toInt(), -42);
    QVERIFY(!controller.statusIsError());
    qunsetenv("NORTHSTAR_WIFI_AUTH_COMMAND");
}

void WifiControllerTest::connectsWithoutWritingTheSecretToTheRequest()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("NORTHSTAR_WIFI_AUTH_COMMAND", writeHelper(directory).toUtf8());
    WifiController controller;
    QSignalSpy cleared(&controller, &WifiController::secretsCleared);
    QSignalSpy finished(&controller, &WifiController::connectionFinished);
    QVERIFY(controller.connectNetwork(QStringLiteral("54657374204e6574"),
                                      QStringLiteral("secured"),
                                      QStringLiteral("correct horse")));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QCOMPARE(finished.first().first().toBool(), true);
    QVERIFY(cleared.size() >= 1);
    QVERIFY(!controller.statusIsError());
    qunsetenv("NORTHSTAR_WIFI_AUTH_COMMAND");
}

QTEST_MAIN(WifiControllerTest)
#include "test-wificontroller.moc"
