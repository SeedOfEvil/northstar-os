#pragma once

class ApplicationLauncher;
class ClockController;
class DesktopLayoutController;
class InputController;
class NotificationCenter;
class PinnedApplicationModel;
class PowerController;
class QuickSettingsController;
class SessionController;
class SettingsCatalog;
class ShellState;
class WallpaperController;

// Declares every desktop setting Northstar actually backs, wiring each one to
// the controller that owns the behavior. Kept out of the shell entry point so
// the declaration itself can be tested against real controllers.
void registerDesktopSettings(SettingsCatalog *catalog,
                             ShellState *shellState,
                             QuickSettingsController *quickSettings,
                             InputController *input,
                             NotificationCenter *notifications,
                             DesktopLayoutController *desktopLayout,
                             ApplicationLauncher *launcher,
                             PinnedApplicationModel *pinnedApplications,
                             PowerController *power,
                             SessionController *session,
                             WallpaperController *wallpaper,
                             ClockController *clock);
