#include "quicksettingscontroller.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

QuickSettingsCommandResult result(int exitCode, const QString &output = {})
{
    return {true, exitCode, output, {}};
}

} // namespace

class QuickSettingsControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void reportsConfirmedCapabilities();
    void confirmsMixerMutations();
    void rejectsUnconfirmedMixerMutations();
    void persistsDoNotDisturb();
};

void QuickSettingsControllerTest::reportsConfirmedCapabilities()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto provider = [](const QString &program, const QStringList &arguments) {
        if (program == QStringLiteral("/sbin/ifconfig")
            && arguments == QStringList{QStringLiteral("-l")}) {
            return result(0, QStringLiteral("lo0 em0 wlan0"));
        }
        if (program == QStringLiteral("/sbin/ifconfig")) {
            return result(0, QStringLiteral("ssid NorthstarLab\nstatus: active"));
        }
        if (program == QStringLiteral("/usr/sbin/hccontrol")) {
            return result(0, QStringLiteral("Node list"));
        }
        if (program == QStringLiteral("/usr/sbin/mixer")) {
            return result(0, QStringLiteral("vol.volume=0.65:0.65"));
        }
        if (program == QStringLiteral("/sbin/sysctl")) {
            return result(0, QStringLiteral("72"));
        }
        return result(1);
    };

    QuickSettingsController controller(
        nullptr, directory.filePath(QStringLiteral("preferences.ini")), provider);
    QVERIFY(controller.wifiAvailable());
    QVERIFY(controller.wifiEnabled());
    QCOMPARE(controller.wifiStatus(), QStringLiteral("Connected to NorthstarLab"));
    QVERIFY(controller.bluetoothAvailable());
    QVERIFY(controller.soundAvailable());
    QCOMPARE(controller.volume(), 65);
    QVERIFY(controller.displayAvailable());
    QCOMPARE(controller.displayBrightness(), 72);
    QVERIFY(!controller.nightLightAvailable());
}

void QuickSettingsControllerTest::confirmsMixerMutations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    int mixerVolume = 30;
    const auto provider = [&mixerVolume](const QString &program, const QStringList &arguments) {
        if (program == QStringLiteral("/usr/sbin/mixer")) {
            if (arguments.size() == 2 && arguments.at(1).startsWith(QStringLiteral("vol.volume="))) {
                mixerVolume = qRound(arguments.at(1).section(QLatin1Char('='), 1).toDouble() * 100.0);
                return result(0);
            }
            return result(0, QStringLiteral("vol.volume=%1").arg(mixerVolume / 100.0, 0, 'f', 2));
        }
        return result(1);
    };

    QuickSettingsController controller(
        nullptr, directory.filePath(QStringLiteral("preferences.ini")), provider);
    QVERIFY(controller.soundAvailable());
    QVERIFY(controller.setVolume(74));
    QCOMPARE(controller.volume(), 74);
    QVERIFY(controller.statusMessage().contains(QStringLiteral("confirmed")));
}

void QuickSettingsControllerTest::rejectsUnconfirmedMixerMutations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto provider = [](const QString &program, const QStringList &) {
        if (program == QStringLiteral("/usr/sbin/mixer")) {
            return result(0, QStringLiteral("vol.volume=0.25"));
        }
        return result(1);
    };

    QuickSettingsController controller(
        nullptr, directory.filePath(QStringLiteral("preferences.ini")), provider);
    QVERIFY(!controller.setVolume(80));
    QCOMPARE(controller.volume(), 25);
    QVERIFY(controller.statusMessage().contains(QStringLiteral("did not confirm")));
}

void QuickSettingsControllerTest::persistsDoNotDisturb()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("preferences.ini"));
    const auto unavailableProvider = [](const QString &, const QStringList &) {
        return result(1);
    };

    {
        QuickSettingsController controller(nullptr, settingsPath, unavailableProvider);
        QVERIFY(!controller.doNotDisturb());
        controller.setDoNotDisturb(true);
        QVERIFY(controller.doNotDisturb());
    }
    QuickSettingsController restored(nullptr, settingsPath, unavailableProvider);
    QVERIFY(restored.doNotDisturb());
}

QTEST_MAIN(QuickSettingsControllerTest)
#include "test-quicksettingscontroller.moc"
