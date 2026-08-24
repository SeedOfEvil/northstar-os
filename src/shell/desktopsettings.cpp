#include "desktopsettings.h"

#include "applicationlauncher.h"
#include "clockcontroller.h"
#include "desktoplayoutcontroller.h"
#include "notificationcenter.h"
#include "notificationstore.h"
#include "pinnedapplicationmodel.h"
#include "powercontroller.h"
#include "quicksettingscontroller.h"
#include "sessioncontroller.h"
#include "settingscatalog.h"
#include "shellstate.h"
#include "wallpapercontroller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>

namespace {

QString pidText(qint64 pid)
{
    return pid > 0 ? QString::number(pid) : QStringLiteral("—");
}

QString orDash(const QString &value)
{
    return value.isEmpty() ? QStringLiteral("—") : value;
}

SettingsCatalog::Entry info(const QString &id, const QString &section, const QString &title,
                            const QString &description, const QStringList &keywords,
                            std::function<QVariant()> read)
{
    SettingsCatalog::Entry entry;
    entry.id = id;
    entry.section = section;
    entry.title = title;
    entry.description = description;
    entry.keywords = keywords;
    entry.kind = SettingsCatalog::infoKind();
    entry.read = std::move(read);
    return entry;
}

} // namespace

void registerDesktopSettings(SettingsCatalog *catalog,
                             ShellState *shellState,
                             QuickSettingsController *quickSettings,
                             NotificationCenter *notifications,
                             DesktopLayoutController *desktopLayout,
                             ApplicationLauncher *launcher,
                             PinnedApplicationModel *pinnedApplications,
                             PowerController *power,
                             SessionController *session,
                             WallpaperController *wallpaper,
                             ClockController *clock)
{
    if (!catalog) {
        return;
    }

    catalog->registerSection(QStringLiteral("appearance"), QStringLiteral("Appearance"),
                             QStringLiteral("How the Northstar shell presents its surfaces."));
    catalog->registerSection(QStringLiteral("desktop"), QStringLiteral("Desktop"),
                             QStringLiteral("Desktop icons, Files defaults, and the dock."));
    catalog->registerSection(QStringLiteral("sound"), QStringLiteral("Sound"),
                             QStringLiteral("Output volume, as reported by the system mixer."));
    catalog->registerSection(QStringLiteral("network"), QStringLiteral("Network"),
                             QStringLiteral("Wireless and Bluetooth adapters detected on this system."));
    catalog->registerSection(QStringLiteral("notifications"), QStringLiteral("Notifications"),
                             QStringLiteral("Delivery and quiet hours for shell notifications."));
    catalog->registerSection(QStringLiteral("datetime"), QStringLiteral("Date & Time"),
                             QStringLiteral("The system clock, its timezone, and network time."));
    catalog->registerSection(QStringLiteral("power"), QStringLiteral("Power"),
                             QStringLiteral("Battery, sleep, and lid-close behavior."));
    catalog->registerSection(QStringLiteral("session"), QStringLiteral("Session"),
                             QStringLiteral("The supervised Northstar session and its lifecycle."));
    catalog->registerSection(QStringLiteral("about"), QStringLiteral("About Northstar"),
                             QStringLiteral("Build identity and diagnostics."));

    // --- Appearance --------------------------------------------------------

    if (shellState) {
        SettingsCatalog::Entry dark;
        dark.id = QStringLiteral("appearance.dark");
        dark.section = QStringLiteral("appearance");
        dark.title = QStringLiteral("Dark appearance");
        dark.description = QStringLiteral("Use the dark Northstar design tokens across every surface.");
        dark.keywords = QStringList{QStringLiteral("theme"), QStringLiteral("dark"),
                                    QStringLiteral("light"), QStringLiteral("colour"),
                                    QStringLiteral("color"), QStringLiteral("appearance")};
        dark.kind = SettingsCatalog::toggleKind();
        dark.read = [shellState]() { return QVariant(shellState->darkMode()); };
        dark.write = [shellState](const QVariant &value) {
            shellState->setDarkMode(value.toBool());
            return shellState->darkMode() == value.toBool();
        };
        catalog->registerEntry(dark);
    }

    if (wallpaper) {
        SettingsCatalog::Entry picture;
        picture.id = QStringLiteral("appearance.wallpaper");
        picture.section = QStringLiteral("appearance");
        picture.title = QStringLiteral("Desktop background");
        picture.description =
            QStringLiteral("A picture from your files, or the built-in Northstar background.");
        picture.keywords = QStringList{QStringLiteral("wallpaper"), QStringLiteral("background"),
                                       QStringLiteral("desktop"), QStringLiteral("picture"),
                                       QStringLiteral("image"), QStringLiteral("photo")};
        picture.kind = SettingsCatalog::pathKind();
        picture.emptyLabel = QStringLiteral("Built-in Northstar background");
        picture.nameFilters = QStringList{
            QStringLiteral("Pictures (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"),
            QStringLiteral("All files (*)")};
        picture.read = [wallpaper]() { return QVariant(wallpaper->imagePath()); };
        picture.write = [wallpaper](const QVariant &value) {
            return wallpaper->setImagePath(value.toString());
        };
        // The controller has already worked out why a picture was refused;
        // repeating that reason is better than inventing a vaguer one.
        picture.writeFailureReason = [wallpaper]() { return wallpaper->status(); };
        catalog->registerEntry(picture);

        SettingsCatalog::Entry fit;
        fit.id = QStringLiteral("appearance.wallpaperfit");
        fit.section = QStringLiteral("appearance");
        fit.title = QStringLiteral("Background fit");
        fit.description = QStringLiteral("How the picture is scaled to the screen.");
        fit.keywords = QStringList{QStringLiteral("fit"), QStringLiteral("fill"),
                                   QStringLiteral("stretch"), QStringLiteral("centre"),
                                   QStringLiteral("center"), QStringLiteral("tile"),
                                   QStringLiteral("scale"), QStringLiteral("wallpaper")};
        fit.kind = SettingsCatalog::choiceKind();
        for (const QString &mode : WallpaperController::fitModes()) {
            fit.options.append(
                SettingsCatalog::choiceOption(mode, WallpaperController::fitModeLabel(mode)));
        }
        fit.read = [wallpaper]() { return QVariant(wallpaper->fitMode()); };
        fit.write = [wallpaper](const QVariant &value) {
            return wallpaper->setFitMode(value.toString());
        };
        // Fit only means something once there is a picture to fit.
        fit.available = [wallpaper]() { return wallpaper->hasImage(); };
        fit.unavailableReason = []() {
            return QStringLiteral("No picture is set, so there is nothing to fit.");
        };
        catalog->registerEntry(fit);
    }

    if (quickSettings) {
        SettingsCatalog::Entry brightness;
        brightness.id = QStringLiteral("appearance.brightness");
        brightness.section = QStringLiteral("appearance");
        brightness.title = QStringLiteral("Display brightness");
        brightness.description = QStringLiteral(
            "Set the laptop panel backlight through FreeBSD and read it back to confirm the change.");
        brightness.keywords = QStringList{QStringLiteral("brightness"), QStringLiteral("display"),
                                           QStringLiteral("screen"), QStringLiteral("backlight")};
        brightness.kind = SettingsCatalog::sliderKind();
        brightness.minimum = 1;
        brightness.maximum = 100;
        brightness.unit = QStringLiteral("%");
        brightness.read = [quickSettings]() {
            return QVariant(quickSettings->displayBrightness());
        };
        brightness.write = [quickSettings](const QVariant &value) {
            return quickSettings->setDisplayBrightness(value.toInt());
        };
        brightness.available = [quickSettings]() { return quickSettings->displayWritable(); };
        brightness.unavailableReason = [quickSettings]() {
            return quickSettings->displayAvailable()
                ? QStringLiteral("This display reports brightness but does not allow Northstar to change it.")
                : quickSettings->displayStatus();
        };
        catalog->registerEntry(brightness);

        if (!quickSettings->displayModes().isEmpty()) {
            SettingsCatalog::Entry resolution;
            resolution.id = QStringLiteral("appearance.resolution");
            resolution.section = QStringLiteral("appearance");
            resolution.title = QStringLiteral("Display resolution");
            resolution.description = QStringLiteral(
                "Preview a mode reported by the connected Wayland display. Unconfirmed changes revert after 30 seconds.");
            resolution.keywords = QStringList{QStringLiteral("display"), QStringLiteral("screen"),
                                               QStringLiteral("resolution"), QStringLiteral("mode"),
                                               QStringLiteral("refresh"), QStringLiteral("hertz")};
            resolution.kind = SettingsCatalog::choiceKind();
            for (const QVariant &modeValue : quickSettings->displayModes()) {
                const QVariantMap mode = modeValue.toMap();
                resolution.options.append(SettingsCatalog::choiceOption(
                    mode.value(QStringLiteral("value")).toString(),
                    mode.value(QStringLiteral("label")).toString()));
            }
            resolution.read = [quickSettings]() {
                return QVariant(quickSettings->currentDisplayMode());
            };
            resolution.write = [quickSettings](const QVariant &value) {
                return quickSettings->previewDisplayMode(value.toString());
            };
            resolution.writeFailureReason = [quickSettings]() {
                return quickSettings->statusMessage();
            };
            resolution.available = [quickSettings]() {
                return quickSettings->displayModeWritable();
            };
            resolution.unavailableReason = []() {
                return QStringLiteral("No alternate mode was reported for this display.");
            };
            catalog->registerEntry(resolution);

            SettingsCatalog::Entry keepMode;
            keepMode.id = QStringLiteral("appearance.keepdisplaymode");
            keepMode.section = QStringLiteral("appearance");
            keepMode.title = QStringLiteral("Keep display preview");
            keepMode.description = QStringLiteral("Save the previewed mode for future Northstar sessions.");
            keepMode.keywords = resolution.keywords;
            keepMode.kind = SettingsCatalog::actionKind();
            keepMode.actionLabel = QStringLiteral("Keep");
            keepMode.perform = [quickSettings]() { return quickSettings->keepDisplayMode(); };
            keepMode.available = [quickSettings]() { return quickSettings->displayModePending(); };
            keepMode.unavailableReason = []() {
                return QStringLiteral("No display preview is waiting for confirmation.");
            };
            catalog->registerEntry(keepMode);

            SettingsCatalog::Entry revertMode = keepMode;
            revertMode.id = QStringLiteral("appearance.revertdisplaymode");
            revertMode.title = QStringLiteral("Revert display preview");
            revertMode.description = QStringLiteral("Restore the mode used before the current preview.");
            revertMode.actionLabel = QStringLiteral("Revert");
            revertMode.perform = [quickSettings]() { return quickSettings->revertDisplayMode(); };
            catalog->registerEntry(revertMode);

            catalog->registerEntry(info(
                QStringLiteral("appearance.displaypreview"), QStringLiteral("appearance"),
                QStringLiteral("Display preview status"),
                QStringLiteral("Time remaining before an unconfirmed mode is restored."),
                resolution.keywords,
                [quickSettings]() {
                    return QVariant(quickSettings->displayModePending()
                        ? QStringLiteral("Reverting in %1 seconds")
                              .arg(quickSettings->displayModeSecondsRemaining())
                        : QStringLiteral("No preview pending"));
                }));
        }

        catalog->registerEntry(info(
            QStringLiteral("appearance.nightlight"), QStringLiteral("appearance"),
            QStringLiteral("Night light"),
            QStringLiteral("Compositor colour temperature control, when the compositor provides it."),
            QStringList{QStringLiteral("night"), QStringLiteral("warm"),
                        QStringLiteral("temperature"), QStringLiteral("blue")},
            [quickSettings]() {
                if (!quickSettings->nightLightAvailable()) {
                    return QVariant(quickSettings->nightLightStatus());
                }
                return QVariant(quickSettings->nightLightEnabled() ? QStringLiteral("On")
                                                                   : QStringLiteral("Off"));
            }));
    }

    // --- Desktop -----------------------------------------------------------

    if (shellState) {
        SettingsCatalog::Entry grid;
        grid.id = QStringLiteral("desktop.filesgrid");
        grid.section = QStringLiteral("desktop");
        grid.title = QStringLiteral("Files opens in grid view");
        grid.description = QStringLiteral("New Files windows start as an icon grid instead of a list.");
        grid.keywords = QStringList{QStringLiteral("files"), QStringLiteral("grid"),
                                    QStringLiteral("list"), QStringLiteral("icons"),
                                    QStringLiteral("view")};
        grid.kind = SettingsCatalog::toggleKind();
        grid.read = [shellState]() { return QVariant(shellState->filesGridView()); };
        grid.write = [shellState](const QVariant &value) {
            shellState->setFilesGridView(value.toBool());
            return shellState->filesGridView() == value.toBool();
        };
        catalog->registerEntry(grid);
    }

    if (desktopLayout) {
        SettingsCatalog::Entry reset;
        reset.id = QStringLiteral("desktop.resetlayout");
        reset.section = QStringLiteral("desktop");
        reset.title = QStringLiteral("Reset desktop icon layout");
        reset.description = QStringLiteral("Return every desktop icon to the default column.");
        reset.keywords = QStringList{QStringLiteral("icons"), QStringLiteral("layout"),
                                     QStringLiteral("arrange"), QStringLiteral("reset"),
                                     QStringLiteral("positions")};
        reset.kind = SettingsCatalog::actionKind();
        reset.actionLabel = QStringLiteral("Reset Layout");
        reset.destructive = true;
        reset.perform = [desktopLayout]() {
            desktopLayout->reset();
            return true;
        };
        catalog->registerEntry(reset);
    }

    if (pinnedApplications) {
        catalog->registerEntry(info(
            QStringLiteral("desktop.pinned"), QStringLiteral("desktop"),
            QStringLiteral("Pinned dock applications"),
            QStringLiteral("Pins are reordered from the dock itself."),
            QStringList{QStringLiteral("dock"), QStringLiteral("pinned"), QStringLiteral("pins")},
            [pinnedApplications]() { return QVariant(pinnedApplications->count()); }));
    }

    // --- Sound -------------------------------------------------------------

    if (quickSettings) {
        SettingsCatalog::Entry volume;
        volume.id = QStringLiteral("sound.volume");
        volume.section = QStringLiteral("sound");
        volume.title = QStringLiteral("Output volume");
        volume.description = QStringLiteral("Applied through the system mixer and read back to confirm it took effect.");
        volume.keywords = QStringList{QStringLiteral("volume"), QStringLiteral("sound"),
                                      QStringLiteral("audio"), QStringLiteral("mixer"),
                                      QStringLiteral("loud"), QStringLiteral("mute")};
        volume.kind = SettingsCatalog::sliderKind();
        volume.minimum = 0;
        volume.maximum = 100;
        volume.unit = QStringLiteral("%");
        volume.read = [quickSettings]() { return QVariant(quickSettings->volume()); };
        volume.write = [quickSettings](const QVariant &value) {
            return quickSettings->setVolume(value.toInt());
        };
        volume.available = [quickSettings]() { return quickSettings->soundAvailable(); };
        volume.unavailableReason = [quickSettings]() { return quickSettings->soundStatus(); };
        catalog->registerEntry(volume);

        SettingsCatalog::Entry mute;
        mute.id = QStringLiteral("sound.mute");
        mute.section = QStringLiteral("sound");
        mute.title = QStringLiteral("Mute output");
        mute.description = QStringLiteral("Silence or restore the active output without changing its volume.");
        mute.keywords = QStringList{QStringLiteral("mute"), QStringLiteral("unmute"),
                                    QStringLiteral("sound"), QStringLiteral("audio")};
        mute.kind = SettingsCatalog::toggleKind();
        mute.read = [quickSettings]() { return QVariant(quickSettings->muted()); };
        mute.write = [quickSettings](const QVariant &value) {
            return quickSettings->setMuted(value.toBool());
        };
        mute.available = [quickSettings]() { return quickSettings->soundAvailable(); };
        mute.unavailableReason = [quickSettings]() { return quickSettings->soundStatus(); };
        catalog->registerEntry(mute);

        SettingsCatalog::Entry output;
        output.id = QStringLiteral("sound.output");
        output.section = QStringLiteral("sound");
        output.title = QStringLiteral("Output device");
        output.description = QStringLiteral("Choose where Northstar sends sound.");
        output.keywords = QStringList{QStringLiteral("output"), QStringLiteral("speakers"),
                                      QStringLiteral("headphones"), QStringLiteral("HDMI"),
                                      QStringLiteral("audio"), QStringLiteral("device")};
        output.kind = SettingsCatalog::choiceKind();
        output.optionSource = [quickSettings]() {
            QVariantList options;
            for (const QVariant &candidate : quickSettings->soundOutputs()) {
                const QVariantMap map = candidate.toMap();
                options.append(SettingsCatalog::choiceOption(
                    QString::number(map.value(QStringLiteral("unit")).toInt()),
                    map.value(QStringLiteral("label")).toString()));
            }
            return options;
        };
        output.read = [quickSettings]() {
            return QVariant(quickSettings->soundOutput() < 0
                ? QString() : QString::number(quickSettings->soundOutput()));
        };
        output.write = [quickSettings](const QVariant &value) {
            return quickSettings->setSoundOutput(value.toString().toInt());
        };
        output.available = [quickSettings]() {
            return quickSettings->soundAvailable() && !quickSettings->soundOutputs().isEmpty();
        };
        output.unavailableReason = [quickSettings]() { return quickSettings->soundStatus(); };
        catalog->registerEntry(output);

        SettingsCatalog::Entry balance;
        balance.id = QStringLiteral("sound.balance");
        balance.section = QStringLiteral("sound");
        balance.title = QStringLiteral("Stereo balance");
        balance.description = QStringLiteral("Shift output toward the left or right channel.");
        balance.keywords = QStringList{QStringLiteral("balance"), QStringLiteral("left"),
                                       QStringLiteral("right"), QStringLiteral("stereo"),
                                       QStringLiteral("audio")};
        balance.kind = SettingsCatalog::sliderKind();
        balance.minimum = -100;
        balance.maximum = 100;
        balance.read = [quickSettings]() { return QVariant(quickSettings->balance()); };
        balance.write = [quickSettings](const QVariant &value) {
            return quickSettings->setBalance(value.toInt());
        };
        balance.available = [quickSettings]() { return quickSettings->soundAvailable(); };
        balance.unavailableReason = [quickSettings]() { return quickSettings->soundStatus(); };
        catalog->registerEntry(balance);

        SettingsCatalog::Entry test;
        test.id = QStringLiteral("sound.test");
        test.section = QStringLiteral("sound");
        test.title = QStringLiteral("Test selected output");
        test.description = QStringLiteral("Play a brief, low-volume tone through the selected device.");
        test.keywords = QStringList{QStringLiteral("test"), QStringLiteral("tone"),
                                    QStringLiteral("speaker"), QStringLiteral("audio")};
        test.kind = SettingsCatalog::actionKind();
        test.actionLabel = QStringLiteral("Play test sound");
        test.perform = [quickSettings]() { return quickSettings->testSound(); };
        test.available = [quickSettings]() {
            return quickSettings->soundAvailable() && quickSettings->testSoundAvailable();
        };
        test.unavailableReason = [quickSettings]() {
            return quickSettings->soundAvailable()
                ? QStringLiteral("The Northstar test tone is not installed.")
                : quickSettings->soundStatus();
        };
        catalog->registerEntry(test);
    }

    // --- Network -----------------------------------------------------------

    if (quickSettings) {
        const QStringList wifiKeywords{QStringLiteral("wifi"), QStringLiteral("wireless"),
                                       QStringLiteral("network"), QStringLiteral("internet")};
        const QStringList bluetoothKeywords{QStringLiteral("bluetooth"),
                                            QStringLiteral("adapter"), QStringLiteral("pair")};

        // A radio becomes a real toggle only where the privileged boundary is
        // installed. Without it the entry stays a reading, because a control
        // that cannot act is worse than an honest observation.
        if (QuickSettingsController::radioControlAvailable()) {
            SettingsCatalog::Entry wifi;
            wifi.id = QStringLiteral("network.wifi");
            wifi.section = QStringLiteral("network");
            wifi.title = QStringLiteral("Wi-Fi");
            wifi.description = QStringLiteral("Bring the wireless interface up or down.");
            wifi.keywords = wifiKeywords;
            wifi.kind = SettingsCatalog::toggleKind();
            wifi.read = [quickSettings]() { return QVariant(quickSettings->wifiEnabled()); };
            wifi.write = [quickSettings](const QVariant &value) {
                return quickSettings->setWifiEnabled(value.toBool());
            };
            wifi.available = [quickSettings]() { return quickSettings->wifiWritable(); };
            wifi.unavailableReason = [quickSettings]() { return quickSettings->wifiStatus(); };
            catalog->registerEntry(wifi);

            SettingsCatalog::Entry bluetooth;
            bluetooth.id = QStringLiteral("network.bluetooth");
            bluetooth.section = QStringLiteral("network");
            bluetooth.title = QStringLiteral("Bluetooth");
            bluetooth.description = QStringLiteral("Start or stop the Bluetooth stack.");
            bluetooth.keywords = bluetoothKeywords;
            bluetooth.kind = SettingsCatalog::toggleKind();
            bluetooth.read = [quickSettings]() { return QVariant(quickSettings->bluetoothEnabled()); };
            bluetooth.write = [quickSettings](const QVariant &value) {
                return quickSettings->setBluetoothEnabled(value.toBool());
            };
            bluetooth.available = [quickSettings]() { return quickSettings->bluetoothWritable(); };
            bluetooth.unavailableReason = [quickSettings]() {
                return quickSettings->bluetoothStatus();
            };
            catalog->registerEntry(bluetooth);
        } else {
            SettingsCatalog::Entry wifi = info(
                QStringLiteral("network.wifi"), QStringLiteral("network"),
                QStringLiteral("Wi-Fi"),
                QStringLiteral("Observed wireless state. Radio control is not installed."),
                wifiKeywords,
                [quickSettings]() {
                    if (!quickSettings->wifiAvailable()) {
                        return QVariant(quickSettings->wifiStatus());
                    }
                    return QVariant(quickSettings->wifiEnabled() ? QStringLiteral("On")
                                                                 : QStringLiteral("Off"));
                });
            wifi.available = [quickSettings]() { return quickSettings->wifiAvailable(); };
            wifi.unavailableReason = [quickSettings]() { return quickSettings->wifiStatus(); };
            catalog->registerEntry(wifi);

            SettingsCatalog::Entry bluetooth = info(
                QStringLiteral("network.bluetooth"), QStringLiteral("network"),
                QStringLiteral("Bluetooth"),
                QStringLiteral("Observed adapter state. Radio control is not installed."),
                bluetoothKeywords,
                [quickSettings]() {
                    if (!quickSettings->bluetoothAvailable()) {
                        return QVariant(quickSettings->bluetoothStatus());
                    }
                    return QVariant(quickSettings->bluetoothEnabled() ? QStringLiteral("On")
                                                                      : QStringLiteral("Off"));
                });
            bluetooth.available = [quickSettings]() { return quickSettings->bluetoothAvailable(); };
            bluetooth.unavailableReason = [quickSettings]() {
                return quickSettings->bluetoothStatus();
            };
            catalog->registerEntry(bluetooth);
        }
    }

    SettingsCatalog::Entry chooseWifi;
    chooseWifi.id = QStringLiteral("network.choose");
    chooseWifi.section = QStringLiteral("network");
    chooseWifi.title = QStringLiteral("Choose Wi-Fi network");
    chooseWifi.description = QStringLiteral("Scan nearby networks and connect with a protected password prompt.");
    chooseWifi.keywords = QStringList{QStringLiteral("wifi"), QStringLiteral("wireless"),
                                      QStringLiteral("ssid"), QStringLiteral("connect"),
                                      QStringLiteral("network")};
    chooseWifi.kind = SettingsCatalog::actionKind();
    chooseWifi.actionLabel = QStringLiteral("Choose Network");
    chooseWifi.available = []() {
        const QString override = qEnvironmentVariable("NORTHSTAR_WIFI_WIZARD");
        return !override.isEmpty() ? QFileInfo::exists(override)
            : !QStandardPaths::findExecutable(QStringLiteral("northstar-wifi")).isEmpty();
    };
    chooseWifi.unavailableReason = []() {
        return QStringLiteral("The Northstar Wi-Fi wizard is not installed.");
    };
    chooseWifi.perform = []() {
        QString program = qEnvironmentVariable("NORTHSTAR_WIFI_WIZARD");
        if (program.isEmpty())
            program = QStandardPaths::findExecutable(QStringLiteral("northstar-wifi"));
        return !program.isEmpty() && QProcess::startDetached(program, {});
    };
    catalog->registerEntry(chooseWifi);

    SettingsCatalog::Entry manageBluetooth;
    manageBluetooth.id = QStringLiteral("network.bluetooth.manage");
    manageBluetooth.section = QStringLiteral("network");
    manageBluetooth.title = QStringLiteral("Manage Bluetooth devices");
    manageBluetooth.description = QStringLiteral(
        "Scan discoverable devices and open the foreground FreeBSD pairing wizard.");
    manageBluetooth.keywords = QStringList{QStringLiteral("bluetooth"), QStringLiteral("pair"),
                                           QStringLiteral("keyboard"), QStringLiteral("mouse"),
                                           QStringLiteral("device")};
    manageBluetooth.kind = SettingsCatalog::actionKind();
    manageBluetooth.actionLabel = QStringLiteral("Manage Devices");
    manageBluetooth.available = []() {
        const QString override = qEnvironmentVariable("NORTHSTAR_BLUETOOTH_WIZARD");
        return !override.isEmpty() ? QFileInfo::exists(override)
            : !QStandardPaths::findExecutable(QStringLiteral("northstar-bluetooth")).isEmpty();
    };
    manageBluetooth.unavailableReason = []() {
        return QStringLiteral("The Northstar Bluetooth wizard is not installed.");
    };
    manageBluetooth.perform = []() {
        QString program = qEnvironmentVariable("NORTHSTAR_BLUETOOTH_WIZARD");
        if (program.isEmpty())
            program = QStandardPaths::findExecutable(QStringLiteral("northstar-bluetooth"));
        return !program.isEmpty() && QProcess::startDetached(program, {});
    };
    catalog->registerEntry(manageBluetooth);

    // --- Notifications -----------------------------------------------------

    if (quickSettings) {
        SettingsCatalog::Entry disturb;
        disturb.id = QStringLiteral("notifications.donotdisturb");
        disturb.section = QStringLiteral("notifications");
        disturb.title = QStringLiteral("Do Not Disturb");
        disturb.description = QStringLiteral("Hold new notifications instead of showing them.");
        disturb.keywords = QStringList{QStringLiteral("quiet"), QStringLiteral("silence"),
                                       QStringLiteral("dnd"), QStringLiteral("focus"),
                                       QStringLiteral("disturb")};
        disturb.kind = SettingsCatalog::toggleKind();
        disturb.read = [quickSettings]() { return QVariant(quickSettings->doNotDisturb()); };
        disturb.write = [quickSettings](const QVariant &value) {
            quickSettings->setDoNotDisturb(value.toBool());
            return quickSettings->doNotDisturb() == value.toBool();
        };
        catalog->registerEntry(disturb);
    }

    if (notifications) {
        catalog->registerEntry(info(
            QStringLiteral("notifications.unread"), QStringLiteral("notifications"),
            QStringLiteral("Unread notifications"),
            QStringLiteral("Currently waiting in the notification centre."),
            QStringList{QStringLiteral("unread"), QStringLiteral("count"),
                        QStringLiteral("badge")},
            [notifications]() { return QVariant(notifications->unreadCount()); }));

        catalog->registerEntry(info(
            QStringLiteral("notifications.history"), QStringLiteral("notifications"),
            QStringLiteral("Notification history"),
            QStringLiteral("Kept in this account's own history file and restored at login."),
            QStringList{QStringLiteral("history"), QStringLiteral("persist"),
                        QStringLiteral("stored"), QStringLiteral("restart")},
            [notifications]() {
                return QVariant(QStringLiteral("%1 kept, %2 days")
                                    .arg(notifications->entries().size())
                                    .arg(NotificationStore::retentionDays()));
            }));

        SettingsCatalog::Entry clear;
        clear.id = QStringLiteral("notifications.clear");
        clear.section = QStringLiteral("notifications");
        clear.title = QStringLiteral("Clear all notifications");
        clear.description = QStringLiteral("Discard every notification, on screen and on disk.");
        clear.keywords = QStringList{QStringLiteral("clear"), QStringLiteral("dismiss"),
                                     QStringLiteral("empty")};
        clear.kind = SettingsCatalog::actionKind();
        clear.actionLabel = QStringLiteral("Clear All");
        clear.destructive = true;
        clear.perform = [notifications]() {
            notifications->clearNotifications();
            return true;
        };
        catalog->registerEntry(clear);
    }

    // --- Power -------------------------------------------------------------

    if (power) {
        catalog->registerEntry(info(
            QStringLiteral("power.battery"), QStringLiteral("power"),
            QStringLiteral("Battery"),
            QStringLiteral("Current battery charge and charging state reported by FreeBSD ACPI."),
            QStringList{QStringLiteral("battery"), QStringLiteral("charge"),
                        QStringLiteral("charging"), QStringLiteral("remaining")},
            [power]() { return QVariant(power->batteryStatus()); }));
        catalog->registerEntry(info(
            QStringLiteral("power.source"), QStringLiteral("power"),
            QStringLiteral("Power source"),
            QStringLiteral("Whether the computer is using its charger or battery."),
            QStringList{QStringLiteral("power"), QStringLiteral("charger"),
                        QStringLiteral("adapter"), QStringLiteral("ac")},
            [power]() {
                if (!power->batteryAvailable()) {
                    return QVariant(QStringLiteral("Battery information unavailable"));
                }
                return QVariant(power->onAcPower() ? QStringLiteral("Power adapter")
                                                    : QStringLiteral("Battery"));
            }));

        SettingsCatalog::Entry lidSleep;
        lidSleep.id = QStringLiteral("power.lidsuspend");
        lidSleep.section = QStringLiteral("power");
        lidSleep.title = QStringLiteral("Sleep when lid closes");
        lidSleep.description = QStringLiteral(
            "Persist the FreeBSD ACPI lid action and apply it immediately.");
        lidSleep.keywords = QStringList{QStringLiteral("lid"), QStringLiteral("close"),
                                        QStringLiteral("sleep"), QStringLiteral("suspend"),
                                        QStringLiteral("laptop")};
        lidSleep.kind = SettingsCatalog::toggleKind();
        lidSleep.read = [power]() { return QVariant(power->lidSuspendEnabled()); };
        lidSleep.write = [power](const QVariant &value) {
            return power->setLidSuspendEnabled(value.toBool());
        };
        lidSleep.available = [power]() { return power->lidSwitchAvailable(); };
        lidSleep.unavailableReason = []() {
            return QStringLiteral("A configurable FreeBSD ACPI lid switch was not detected.");
        };
        lidSleep.writeFailureReason = [power]() { return power->statusMessage(); };
        catalog->registerEntry(lidSleep);
    }

    // --- Date & Time -------------------------------------------------------

    if (clock) {
        catalog->registerEntry(info(
            QStringLiteral("datetime.current"), QStringLiteral("datetime"),
            QStringLiteral("Current time"),
            QStringLiteral("The system clock as this desktop reads it."),
            QStringList{QStringLiteral("clock"), QStringLiteral("time"),
                        QStringLiteral("date"), QStringLiteral("now")},
            []() {
                return QVariant(QDateTime::currentDateTime().toString(
                    QStringLiteral("ddd d MMM yyyy, HH:mm t")));
            }));

        const QStringList regions = clock->regions();
        if (regions.isEmpty()) {
            // No zoneinfo database means there is no timezone to choose. Say
            // so rather than leaving the section silently short of controls.
            catalog->registerEntry(info(
                QStringLiteral("datetime.timezone"), QStringLiteral("datetime"),
                QStringLiteral("Timezone"),
                QStringLiteral("Read from the system zoneinfo database."),
                QStringList{QStringLiteral("timezone"), QStringLiteral("zone"),
                            QStringLiteral("region"), QStringLiteral("utc")},
                []() {
                    return QVariant(QStringLiteral("No zoneinfo database on this system"));
                }));
        } else {
            SettingsCatalog::Entry region;
            region.id = QStringLiteral("datetime.region");
            region.section = QStringLiteral("datetime");
            region.title = QStringLiteral("Region");
            region.description =
                QStringLiteral("Which part of the world to list timezones from.");
            region.keywords = QStringList{QStringLiteral("region"), QStringLiteral("continent"),
                                          QStringLiteral("timezone"), QStringLiteral("zone")};
            region.kind = SettingsCatalog::choiceKind();
            for (const QString &name : regions) {
                region.options.append(SettingsCatalog::choiceOption(name, name));
            }
            region.read = [clock]() { return QVariant(clock->region()); };
            region.write = [clock](const QVariant &value) {
                clock->setRegion(value.toString());
                return clock->region() == value.toString();
            };
            catalog->registerEntry(region);

            SettingsCatalog::Entry zone;
            zone.id = QStringLiteral("datetime.timezone");
            zone.section = QStringLiteral("datetime");
            zone.title = QStringLiteral("Timezone");
            zone.description = QStringLiteral("The zone the system clock is set to.");
            zone.keywords = QStringList{QStringLiteral("timezone"), QStringLiteral("zone"),
                                        QStringLiteral("city"), QStringLiteral("utc"),
                                        QStringLiteral("clock")};
            zone.kind = SettingsCatalog::choiceKind();
            // A system that has never recorded its timezone reads back
            // nothing, and the honest answer is to show that rather than
            // name a zone it is not actually in.
            zone.allowsUnset = true;
            // The zone list follows the chosen region, so it is asked for
            // rather than fixed when the catalog is built.
            zone.optionSource = [clock]() {
                QVariantList options;
                for (const QString &name : clock->selectableZones()) {
                    options.append(SettingsCatalog::choiceOption(name, name));
                }
                return options;
            };
            zone.read = [clock]() { return QVariant(clock->timeZone()); };
            zone.write = [clock](const QVariant &value) {
                return clock->setTimeZone(value.toString());
            };
            zone.available = [clock]() { return clock->timeZoneWritable(); };
            zone.unavailableReason = []() {
                return QStringLiteral("The clock boundary is not installed on this system.");
            };
            zone.writeFailureReason = [clock]() { return clock->status(); };
            catalog->registerEntry(zone);
        }

        SettingsCatalog::Entry networkTime;
        networkTime.id = QStringLiteral("datetime.ntp");
        networkTime.section = QStringLiteral("datetime");
        networkTime.title = QStringLiteral("Set time automatically");
        networkTime.description =
            QStringLiteral("Keep the clock set from the network time servers this system "
                           "is configured with.");
        networkTime.keywords = QStringList{QStringLiteral("ntp"), QStringLiteral("network"),
                                           QStringLiteral("automatic"), QStringLiteral("sync"),
                                           QStringLiteral("time")};
        networkTime.kind = SettingsCatalog::toggleKind();
        networkTime.read = [clock]() { return QVariant(clock->ntpEnabled()); };
        networkTime.write = [clock](const QVariant &value) {
            return clock->setNtpEnabled(value.toBool());
        };
        networkTime.available = [clock]() { return clock->ntpWritable(); };
        networkTime.unavailableReason = [clock]() { return clock->ntpStatus(); };
        networkTime.writeFailureReason = [clock]() { return clock->status(); };
        catalog->registerEntry(networkTime);

        catalog->registerEntry(info(
            QStringLiteral("datetime.ntpstate"), QStringLiteral("datetime"),
            QStringLiteral("Network time status"),
            QStringLiteral("What the time daemon is doing right now."),
            QStringList{QStringLiteral("ntp"), QStringLiteral("daemon"),
                        QStringLiteral("status"), QStringLiteral("running")},
            [clock]() { return QVariant(clock->ntpStatus()); }));

        SettingsCatalog::Entry syncNow;
        syncNow.id = QStringLiteral("datetime.sync");
        syncNow.section = QStringLiteral("datetime");
        syncNow.title = QStringLiteral("Set the clock now");
        syncNow.description =
            QStringLiteral("Correct the clock once from the network, without leaving the "
                           "time daemon running.");
        syncNow.keywords = QStringList{QStringLiteral("sync"), QStringLiteral("correct"),
                                       QStringLiteral("now"), QStringLiteral("ntp"),
                                       QStringLiteral("clock")};
        syncNow.kind = SettingsCatalog::actionKind();
        syncNow.actionLabel = QStringLiteral("Set now");
        syncNow.perform = [clock]() { return clock->synchroniseNow(); };
        // A one-shot step has nothing to do while the daemon already holds
        // the clock, so it is offered only when it would actually act.
        syncNow.available = [clock]() {
            return clock->ntpWritable() && !clock->ntpRunning() && !clock->synchronising();
        };
        syncNow.unavailableReason = [clock]() {
            if (clock->synchronising()) {
                return QStringLiteral("Setting the clock from the network...");
            }
            return clock->ntpRunning()
                ? QStringLiteral("Network time is already running and keeping the clock set.")
                : clock->ntpStatus();
        };
        catalog->registerEntry(syncNow);
    }

    // --- Session -----------------------------------------------------------

    if (session) {
        const QStringList sessionKeywords{QStringLiteral("session"), QStringLiteral("supervisor"),
                                          QStringLiteral("compositor")};

        catalog->registerEntry(info(
            QStringLiteral("session.state"), QStringLiteral("session"),
            QStringLiteral("Session state"),
            QStringLiteral("Reported by the session supervisor."), sessionKeywords,
            [session]() {
                return QVariant(session->available() ? orDash(session->state())
                                                     : QStringLiteral("Not supervised"));
            }));

        catalog->registerEntry(info(
            QStringLiteral("session.display"), QStringLiteral("session"),
            QStringLiteral("Wayland display"), QStringLiteral("The compositor socket this session uses."),
            QStringList{QStringLiteral("wayland"), QStringLiteral("display"),
                        QStringLiteral("socket")},
            [session]() {
                return QVariant(session->available() ? orDash(session->waylandDisplay())
                                                     : QStringLiteral("—"));
            }));

        catalog->registerEntry(info(
            QStringLiteral("session.supervisorpid"), QStringLiteral("session"),
            QStringLiteral("Supervisor PID"), QStringLiteral("Process supervising this session."),
            sessionKeywords, [session]() { return QVariant(pidText(session->supervisorPid())); }));

        catalog->registerEntry(info(
            QStringLiteral("session.shellpid"), QStringLiteral("session"),
            QStringLiteral("Shell PID"), QStringLiteral("The running Northstar shell process."),
            sessionKeywords, [session]() { return QVariant(pidText(session->shellPid())); }));

        catalog->registerEntry(info(
            QStringLiteral("session.compositorpid"), QStringLiteral("session"),
            QStringLiteral("Compositor PID"), QStringLiteral("The running compositor process."),
            sessionKeywords, [session]() { return QVariant(pidText(session->compositorPid())); }));

        catalog->registerEntry(info(
            QStringLiteral("session.restarts"), QStringLiteral("session"),
            QStringLiteral("Shell restarts"),
            QStringLiteral("Times the supervisor has restarted the shell in this session."),
            QStringList{QStringLiteral("restart"), QStringLiteral("count")},
            [session]() { return QVariant(session->restartCount()); }));

        catalog->registerEntry(info(
            QStringLiteral("session.lastevent"), QStringLiteral("session"),
            QStringLiteral("Last session event"), QStringLiteral("Most recent supervisor event."),
            QStringList{QStringLiteral("event"), QStringLiteral("log")},
            [session]() { return QVariant(orDash(session->lastEvent())); }));

        SettingsCatalog::Entry restart;
        restart.id = QStringLiteral("session.restartshell");
        restart.section = QStringLiteral("session");
        restart.title = QStringLiteral("Restart Northstar shell");
        restart.description = QStringLiteral("Restart only the shell. Running applications keep running.");
        restart.keywords = QStringList{QStringLiteral("restart"), QStringLiteral("shell"),
                                       QStringLiteral("reload")};
        restart.kind = SettingsCatalog::actionKind();
        restart.actionLabel = QStringLiteral("Restart Shell");
        restart.destructive = true;
        restart.perform = [session]() { return session->requestShellRestart(); };
        restart.available = [session]() { return session->restartable(); };
        restart.unavailableReason = []() {
            return QStringLiteral("This session is not supervised, so the shell cannot be restarted from here.");
        };
        catalog->registerEntry(restart);

        SettingsCatalog::Entry endSession;
        endSession.id = QStringLiteral("session.end");
        endSession.section = QStringLiteral("session");
        endSession.title = QStringLiteral("End Northstar session");
        endSession.description = QStringLiteral("Log out and return to the display manager.");
        endSession.keywords = QStringList{QStringLiteral("logout"), QStringLiteral("log out"),
                                          QStringLiteral("end"), QStringLiteral("quit"),
                                          QStringLiteral("sign out")};
        endSession.kind = SettingsCatalog::actionKind();
        endSession.actionLabel = QStringLiteral("End Session");
        endSession.destructive = true;
        endSession.perform = [session]() { return session->requestEndSession(); };
        endSession.available = [session]() { return session->available(); };
        endSession.unavailableReason = []() {
            return QStringLiteral("This session is not supervised, so it cannot be ended from here.");
        };
        catalog->registerEntry(endSession);
    }

    if (launcher) {
        SettingsCatalog::Entry refresh;
        refresh.id = QStringLiteral("session.refreshcatalog");
        refresh.section = QStringLiteral("session");
        refresh.title = QStringLiteral("Refresh application catalog");
        refresh.description = QStringLiteral("Re-read installed application bundles and desktop entries.");
        refresh.keywords = QStringList{QStringLiteral("applications"), QStringLiteral("catalog"),
                                       QStringLiteral("refresh"), QStringLiteral("rescan")};
        refresh.kind = SettingsCatalog::actionKind();
        refresh.actionLabel = QStringLiteral("Refresh Catalog");
        refresh.perform = [launcher]() {
            launcher->refreshApplications();
            // Finding no changes is a successful refresh, not a failure.
            return true;
        };
        catalog->registerEntry(refresh);
    }

    // --- About -------------------------------------------------------------

    catalog->registerEntry(info(
        QStringLiteral("about.version"), QStringLiteral("about"), QStringLiteral("Shell version"),
        QStringLiteral("The running Northstar shell build."),
        QStringList{QStringLiteral("version"), QStringLiteral("build"), QStringLiteral("about")},
        []() {
            return QVariant(QStringLiteral("%1 %2")
                                .arg(QCoreApplication::applicationName(),
                                     QCoreApplication::applicationVersion()));
        }));

    if (launcher) {
        catalog->registerEntry(info(
            QStringLiteral("about.applications"), QStringLiteral("about"),
            QStringLiteral("Applications available"),
            QStringLiteral("Desktop entries and application bundles the launcher can start."),
            QStringList{QStringLiteral("applications"), QStringLiteral("count")},
            [launcher]() { return QVariant(launcher->applications().size()); }));

        catalog->registerEntry(info(
            QStringLiteral("about.launchlog"), QStringLiteral("about"),
            QStringLiteral("Launch log"),
            QStringLiteral("Where the shell records application launches."),
            QStringList{QStringLiteral("log"), QStringLiteral("diagnostics"),
                        QStringLiteral("launch")},
            [launcher]() { return QVariant(launcher->launchLogPath()); }));
    }
}
