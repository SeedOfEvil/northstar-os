#include "quicksettingscontroller.h"

#include <QFile>
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
    void confirmsMixerMuteMutations();
    void rejectsUnconfirmedMixerMutations();
    void persistsDoNotDisturb();

    void leavesRadiosReadOnlyWithoutTheHelper();
    void togglesARadioThroughTheHelper();
    void reportsAbsentRadioHardware();
    void reportsARefusedRadioChange();
    void refusesToActOnAbsentWireless();
    void reportsAdministrativeStateRatherThanAssociation();
};

namespace {

// Writes an executable stand-in at the given path, so a test can say "the
// boundary is installed here" without depending on whether the real one
// happens to be installed on the machine running the suite.
bool installHelperStub(const QString &path)
{
    QFile stub(path);
    if (!stub.open(QIODevice::WriteOnly)) {
        return false;
    }
    stub.write("#!/bin/sh\nexit 0\n");
    stub.close();
    return QFile::setPermissions(path,
                                 QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

// A stand-in for the privileged boundary. It records what it was asked to do
// and never runs anything, so the writer can be tested without a radio.
struct RadioHelper
{
    QStringList calls;
    int exitCode = 0;
    bool started = true;
};

// The equipped system every radio test starts from: one wireless interface,
// active, and a Bluetooth controller present.
QuickSettingsController::CommandProvider equippedSystem(RadioHelper *helper,
                                                        const QString &helperPath)
{
    return [helper, helperPath](const QString &program, const QStringList &arguments) {
        if (program == helperPath) {
            helper->calls.append(arguments.join(QLatin1Char(' ')));
            return QuickSettingsCommandResult{helper->started, helper->exitCode, {}, {}};
        }
        if (program == QStringLiteral("/sbin/ifconfig")
            && arguments == QStringList{QStringLiteral("-l")}) {
            return result(0, QStringLiteral("lo0 em0 wlan0"));
        }
        if (program == QStringLiteral("/sbin/ifconfig")) {
            return result(0, QStringLiteral(
                "wlan0: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> metric 0 mtu 1500\n"
                "\tssid NorthstarLab channel 6\n"
                "\tstatus: associated"));
        }
        if (program == QStringLiteral("/usr/sbin/hccontrol")) {
            return result(0, QStringLiteral("Node list"));
        }
        return result(1);
    };
}

} // namespace

void QuickSettingsControllerTest::reportsConfirmedCapabilities()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QStringList mixerCalls;
    const auto provider = [&mixerCalls](const QString &program, const QStringList &arguments) {
        if (program == QStringLiteral("/sbin/ifconfig")
            && arguments == QStringList{QStringLiteral("-l")}) {
            return result(0, QStringLiteral("lo0 em0 wlan0"));
        }
        if (program == QStringLiteral("/sbin/ifconfig")) {
            return result(0, QStringLiteral(
                "wlan0: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> metric 0 mtu 1500\n"
                "\tssid NorthstarLab channel 6\n"
                "\tstatus: associated"));
        }
        if (program == QStringLiteral("/usr/sbin/hccontrol")) {
            return result(0, QStringLiteral("Node list"));
        }
        if (program == QStringLiteral("/usr/sbin/mixer")) {
            mixerCalls.append(arguments.join(QLatin1Char(' ')));
            return result(0, QStringLiteral("vol.volume=0.65:0.65\nvol.mute=off"));
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
    QCOMPARE(controller.volume(), 2);
    QVERIFY(!controller.muted());
    QCOMPARE(mixerCalls, QStringList{QStringLiteral("vol")});
    QVERIFY(controller.displayAvailable());
    QCOMPARE(controller.displayBrightness(), 72);
    QVERIFY(!controller.nightLightAvailable());
}

void QuickSettingsControllerTest::confirmsMixerMutations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    int mixerVolume = 30;
    QStringList mixerCalls;
    const auto provider = [&mixerVolume, &mixerCalls](const QString &program, const QStringList &arguments) {
        if (program == QStringLiteral("/usr/sbin/mixer")) {
            mixerCalls.append(arguments.join(QLatin1Char(' ')));
            if (arguments.size() == 1 && arguments.constFirst().startsWith(QStringLiteral("vol.volume="))) {
                mixerVolume = qRound(arguments.constFirst().section(QLatin1Char('='), 1).toDouble() * 100.0);
                return result(0);
            }
            return result(0, QStringLiteral("vol.volume=%1\nvol.mute=off")
                                 .arg(mixerVolume / 100.0, 0, 'f', 2));
        }
        return result(1);
    };

    QuickSettingsController controller(
        nullptr, directory.filePath(QStringLiteral("preferences.ini")), provider);
    QVERIFY(controller.soundAvailable());
    QVERIFY(controller.setVolume(74));
    QCOMPARE(controller.volume(), 72);
    QVERIFY(controller.statusMessage().contains(QStringLiteral("confirmed")));
    QCOMPARE(mixerCalls,
             QStringList({QStringLiteral("vol"),
                          QStringLiteral("vol.volume=0.94"),
                          QStringLiteral("vol")}));
}

void QuickSettingsControllerTest::confirmsMixerMuteMutations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    bool muted = false;
    QStringList mixerCalls;
    const auto provider = [&muted, &mixerCalls](const QString &program, const QStringList &arguments) {
        if (program != QStringLiteral("/usr/sbin/mixer")) {
            return result(1);
        }
        mixerCalls.append(arguments.join(QLatin1Char(' ')));
        if (arguments == QStringList{QStringLiteral("vol.mute=on")}) {
            muted = true;
            return result(0);
        }
        if (arguments == QStringList{QStringLiteral("vol.mute=off")}) {
            muted = false;
            return result(0);
        }
        return result(0, QStringLiteral("vol.volume=0.80:0.80\nvol.mute=%1")
                             .arg(muted ? QStringLiteral("on") : QStringLiteral("off")));
    };

    QuickSettingsController controller(
        nullptr, directory.filePath(QStringLiteral("preferences.ini")), provider);
    QVERIFY(!controller.muted());
    QVERIFY(controller.setMuted(true));
    QVERIFY(controller.muted());
    QVERIFY(controller.setMuted(false));
    QVERIFY(!controller.muted());
    QCOMPARE(mixerCalls,
             QStringList({QStringLiteral("vol"),
                          QStringLiteral("vol.mute=on"),
                          QStringLiteral("vol"),
                          QStringLiteral("vol.mute=off"),
                          QStringLiteral("vol")}));
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
    QCOMPARE(controller.volume(), 0);
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

void QuickSettingsControllerTest::leavesRadiosReadOnlyWithoutTheHelper()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString absent = directory.filePath(QStringLiteral("northstar-radio"));
    qputenv("NORTHSTAR_RADIO_HELPER", absent.toUtf8());

    RadioHelper helper;
    QuickSettingsController controller(nullptr, directory.filePath(QStringLiteral("quick.ini")),
                                       equippedSystem(&helper, absent));

    // The path is configured but nothing is installed there. The hardware is
    // present, yet no control may be offered, and the override must stay
    // authoritative rather than falling through to whatever this machine has
    // installed -- otherwise the result depends on the build host.
    QVERIFY(controller.wifiAvailable());
    QVERIFY(!QuickSettingsController::radioControlAvailable());
    QVERIFY(!controller.wifiWritable());
    QVERIFY(!controller.bluetoothWritable());
    QVERIFY(!controller.setWifiEnabled(false));
    QVERIFY(helper.calls.isEmpty());

    qunsetenv("NORTHSTAR_RADIO_HELPER");
}

void QuickSettingsControllerTest::togglesARadioThroughTheHelper()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString helperPath = directory.filePath(QStringLiteral("northstar-radio"));
    QVERIFY(installHelperStub(helperPath));
    qputenv("NORTHSTAR_RADIO_HELPER", helperPath.toUtf8());

    RadioHelper helper;
    QuickSettingsController controller(nullptr, directory.filePath(QStringLiteral("quick.ini")),
                                       equippedSystem(&helper, helperPath));

    QVERIFY(QuickSettingsController::radioControlAvailable());
    QVERIFY(controller.wifiWritable());
    QVERIFY(controller.bluetoothWritable());

    QVERIFY(controller.setWifiEnabled(false));
    QCOMPARE(helper.calls.size(), 1);
    QCOMPARE(helper.calls.first(), QStringLiteral("wifi off"));

    QVERIFY(controller.setBluetoothEnabled(true));
    QCOMPARE(helper.calls.size(), 2);
    QCOMPARE(helper.calls.last(), QStringLiteral("bluetooth on"));

    // Two fixed words, always. Nothing else is ever handed to the boundary.
    for (const QString &call : helper.calls) {
        QCOMPARE(call.split(QLatin1Char(' ')).size(), 2);
    }

    qunsetenv("NORTHSTAR_RADIO_HELPER");
}

void QuickSettingsControllerTest::reportsAbsentRadioHardware()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString helperPath = directory.filePath(QStringLiteral("northstar-radio"));
    QVERIFY(installHelperStub(helperPath));
    qputenv("NORTHSTAR_RADIO_HELPER", helperPath.toUtf8());

    RadioHelper helper;
    helper.exitCode = 69;
    QuickSettingsController controller(nullptr, directory.filePath(QStringLiteral("quick.ini")),
                                       equippedSystem(&helper, helperPath));

    QVERIFY(!controller.setBluetoothEnabled(false));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("hardware")));

    qunsetenv("NORTHSTAR_RADIO_HELPER");
}

void QuickSettingsControllerTest::reportsARefusedRadioChange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString helperPath = directory.filePath(QStringLiteral("northstar-radio"));
    QVERIFY(installHelperStub(helperPath));
    qputenv("NORTHSTAR_RADIO_HELPER", helperPath.toUtf8());

    RadioHelper helper;
    helper.exitCode = 1;
    QuickSettingsController controller(nullptr, directory.filePath(QStringLiteral("quick.ini")),
                                       equippedSystem(&helper, helperPath));

    QVERIFY(!controller.setWifiEnabled(false));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("refused")));

    // A boundary that could not be started at all is reported differently from
    // one that ran and said no.
    helper.started = false;
    QVERIFY(!controller.setWifiEnabled(true));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("could not be run")));

    qunsetenv("NORTHSTAR_RADIO_HELPER");
}

void QuickSettingsControllerTest::refusesToActOnAbsentWireless()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString helperPath = directory.filePath(QStringLiteral("northstar-radio"));
    QVERIFY(installHelperStub(helperPath));
    qputenv("NORTHSTAR_RADIO_HELPER", helperPath.toUtf8());

    RadioHelper helper;
    // A machine with no wireless interface at all, which is what the
    // development VM actually is.
    const auto provider = [&helper, helperPath](const QString &program,
                                                const QStringList &arguments) {
        if (program == helperPath) {
            helper.calls.append(arguments.join(QLatin1Char(' ')));
            return QuickSettingsCommandResult{true, 0, {}, {}};
        }
        if (program == QStringLiteral("/sbin/ifconfig")
            && arguments == QStringList{QStringLiteral("-l")}) {
            return result(0, QStringLiteral("lo0 vtnet0"));
        }
        return result(1);
    };

    QuickSettingsController controller(nullptr, directory.filePath(QStringLiteral("quick.ini")),
                                       provider);

    QVERIFY(!controller.wifiAvailable());
    QVERIFY(!controller.wifiWritable());
    QVERIFY(!controller.setWifiEnabled(true));
    QVERIFY(helper.calls.isEmpty());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("No wireless interface")));

    qunsetenv("NORTHSTAR_RADIO_HELPER");
}

void QuickSettingsControllerTest::reportsAdministrativeStateRatherThanAssociation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("NORTHSTAR_RADIO_HELPER",
            directory.filePath(QStringLiteral("absent-radio")).toUtf8());

    const auto systemWith = [](const QString &interfaceBlock) {
        return [interfaceBlock](const QString &program, const QStringList &arguments) {
            if (program == QStringLiteral("/sbin/ifconfig")
                && arguments == QStringList{QStringLiteral("-l")}) {
                return result(0, QStringLiteral("lo0 wlan0"));
            }
            if (program == QStringLiteral("/sbin/ifconfig")) {
                return result(0, interfaceBlock);
            }
            return result(1);
        };
    };

    // Up but not yet associated. This is the state immediately after switching
    // Wi-Fi on, and the control must already read as on.
    QuickSettingsController connecting(
        nullptr, directory.filePath(QStringLiteral("a.ini")),
        systemWith(QStringLiteral(
            "wlan0: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> metric 0 mtu 1500\n"
            "\tstatus: no carrier")));
    QVERIFY(connecting.wifiEnabled());
    QCOMPARE(connecting.wifiStatus(), QStringLiteral("Wireless interface on, not connected"));

    // Down. Associated text can linger in a stale read; the flags decide.
    QuickSettingsController down(
        nullptr, directory.filePath(QStringLiteral("b.ini")),
        systemWith(QStringLiteral(
            "wlan0: flags=8802<BROADCAST,SIMPLEX,MULTICAST> metric 0 mtu 1500\n"
            "\tssid NorthstarLab\n\tstatus: associated")));
    QVERIFY(!down.wifiEnabled());
    QCOMPARE(down.wifiStatus(), QStringLiteral("Wireless interface off"));

    // Up and associated, which is the only state that names the network.
    QuickSettingsController connected(
        nullptr, directory.filePath(QStringLiteral("c.ini")),
        systemWith(QStringLiteral(
            "wlan0: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> metric 0 mtu 1500\n"
            "\tssid NorthstarLab channel 6\n\tstatus: associated")));
    QVERIFY(connected.wifiEnabled());
    QCOMPARE(connected.wifiStatus(), QStringLiteral("Connected to NorthstarLab"));

    // A wired-style "active" driver report is accepted as association too.
    QuickSettingsController active(
        nullptr, directory.filePath(QStringLiteral("d.ini")),
        systemWith(QStringLiteral(
            "wlan0: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> metric 0 mtu 1500\n"
            "\tstatus: active")));
    QVERIFY(active.wifiEnabled());
    QCOMPARE(active.wifiStatus(), QStringLiteral("Wireless link active"));

    qunsetenv("NORTHSTAR_RADIO_HELPER");
}

QTEST_MAIN(QuickSettingsControllerTest)
#include "test-quicksettingscontroller.moc"
