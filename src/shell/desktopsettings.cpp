#include "desktopsettings.h"

#include "applicationlauncher.h"
#include "desktoplayoutcontroller.h"
#include "notificationcenter.h"
#include "notificationstore.h"
#include "pinnedapplicationmodel.h"
#include "quicksettingscontroller.h"
#include "sessioncontroller.h"
#include "settingscatalog.h"
#include "shellstate.h"

#include <QCoreApplication>
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
                             SessionController *session)
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

    if (quickSettings) {
        // Brightness and night light are observed, not driven: this build has
        // no writable backend for either, so they are reported rather than
        // offered as controls that would silently do nothing.
        catalog->registerEntry(info(
            QStringLiteral("appearance.brightness"), QStringLiteral("appearance"),
            QStringLiteral("Display brightness"),
            QStringLiteral("Reported by the display backend. Northstar cannot set it on this system."),
            QStringList{QStringLiteral("brightness"), QStringLiteral("display"),
                        QStringLiteral("screen"), QStringLiteral("backlight")},
            [quickSettings]() {
                return QVariant(quickSettings->displayAvailable()
                                    ? QStringLiteral("%1%").arg(quickSettings->displayBrightness())
                                    : quickSettings->displayStatus());
            }));

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
