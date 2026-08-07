#include "layershellsurface.h"

#include <QString>
#include <QWindow>

#include <LayerShellQt/Window>

bool LayerShellSurface::configurePanel(QWindow *window, QScreen *screen, int exclusiveZone, int displayIndex)
{
    if (window == nullptr || screen == nullptr || exclusiveZone <= 0) {
        return false;
    }

    auto *surface = LayerShellQt::Window::get(window);
    if (surface == nullptr) {
        return false;
    }

    LayerShellQt::Window::Anchors anchors{LayerShellQt::Window::AnchorTop};
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    surface->setAnchors(anchors);
    surface->setExclusiveZone(exclusiveZone);
    surface->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    surface->setLayer(LayerShellQt::Window::LayerTop);
    surface->setScreen(screen);
    surface->setScope(QStringLiteral("northstar-shell-%1").arg(displayIndex));
    surface->setActivateOnShow(false);
    surface->setCloseOnDismissed(true);

    window->setScreen(screen);
    window->setHeight(exclusiveZone);
    return true;
}
