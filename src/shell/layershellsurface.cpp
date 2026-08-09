#include "layershellsurface.h"

#include <QString>
#include <QWindow>

#include <LayerShellQt/Window>

bool LayerShellSurface::configureBackground(QWindow *window, QScreen *screen, int displayIndex)
{
    if (window == nullptr || screen == nullptr) {
        return false;
    }

    auto *surface = LayerShellQt::Window::get(window);
    if (surface == nullptr) {
        return false;
    }

    LayerShellQt::Window::Anchors anchors{LayerShellQt::Window::AnchorTop};
    anchors |= LayerShellQt::Window::AnchorBottom;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    surface->setAnchors(anchors);
    // A background is visual output content, not a normal work-area client.
    // -1 tells layer-shell to cover the physical output even where the top
    // panel and dock reserve space. Those surfaces remain above this layer,
    // while maximized application windows still respect their reservations.
    surface->setExclusiveZone(-1);
    surface->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    surface->setLayer(LayerShellQt::Window::LayerBackground);
    surface->setScreen(screen);
    surface->setScope(QStringLiteral("northstar-background-%1").arg(displayIndex));
    surface->setActivateOnShow(false);
    surface->setCloseOnDismissed(true);

    window->setScreen(screen);
    window->setWidth(screen->geometry().width());
    window->setHeight(screen->geometry().height());
    return true;
}

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
    // The panel contains the global search field and must be focusable when
    // the user clicks it. OnDemand preserves normal compositor focus rules;
    // the desktop background and dock remain keyboard-inert.
    surface->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    surface->setLayer(LayerShellQt::Window::LayerTop);
    surface->setScreen(screen);
    surface->setScope(QStringLiteral("northstar-shell-%1").arg(displayIndex));
    surface->setActivateOnShow(false);
    surface->setCloseOnDismissed(true);

    window->setScreen(screen);
    window->setHeight(exclusiveZone);
    return true;
}

bool LayerShellSurface::configureDock(QWindow *window, QScreen *screen, int exclusiveZone, int displayIndex)
{
    if (window == nullptr || screen == nullptr || exclusiveZone <= 0) {
        return false;
    }

    auto *surface = LayerShellQt::Window::get(window);
    if (surface == nullptr) {
        return false;
    }

    LayerShellQt::Window::Anchors anchors{LayerShellQt::Window::AnchorBottom};
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    surface->setAnchors(anchors);
    surface->setExclusiveZone(exclusiveZone);
    surface->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    surface->setLayer(LayerShellQt::Window::LayerTop);
    surface->setScreen(screen);
    surface->setScope(QStringLiteral("northstar-dock-%1").arg(displayIndex));
    surface->setActivateOnShow(false);
    surface->setCloseOnDismissed(true);

    window->setScreen(screen);
    window->setHeight(exclusiveZone);
    return true;
}
