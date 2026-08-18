#pragma once

#include <QScreen>

class QWindow;

class LayerShellSurface final
{
public:
    static bool configureBackground(QWindow *window, QScreen *screen, int displayIndex);
    static bool configurePanel(QWindow *window, QScreen *screen, int exclusiveZone, int displayIndex);
    static bool configureDock(QWindow *window, QScreen *screen, int exclusiveZone, int displayIndex);

    // Ask the compositor to give keyboard focus back to a configured panel.
    static bool restoreKeyboardFocus(QWindow *window);
};
