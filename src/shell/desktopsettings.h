#pragma once

class ApplicationLauncher;
class DesktopLayoutController;
class NotificationCenter;
class PinnedApplicationModel;
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
                             NotificationCenter *notifications,
                             DesktopLayoutController *desktopLayout,
                             ApplicationLauncher *launcher,
                             PinnedApplicationModel *pinnedApplications,
                             SessionController *session,
                             WallpaperController *wallpaper);
