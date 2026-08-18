#include "layershellsurface.h"

#include <QString>
#include <QTimer>
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

bool LayerShellSurface::restoreKeyboardFocus(QWindow *window)
{
    if (window == nullptr || !window->isVisible()) {
        return false;
    }

    auto *surface = LayerShellQt::Window::get(window);
    if (surface == nullptr) {
        return false;
    }

    // Only one reclaim may be in flight; a second would revert the first.
    static const char *const PendingProperty = "northstarFocusReclaimPending";
    if (window->property(PendingProperty).toBool()) {
        return true;
    }

    // A layer surface only receives keyboard focus when it asks for it. The
    // panel is configured on demand, so the compositor focuses it when the
    // user interacts with it; after an ordinary toplevel closes there is
    // nothing to hand focus back to, and requestActivate() alone is ignored.
    //
    // Declaring exclusive interactivity makes the compositor focus the panel.
    // The exclusive state has to survive until the compositor has actually
    // acted on it: layer-shell requests only take effect on the next surface
    // commit, so setting exclusive and on demand in the same turn coalesces
    // into a single request and the compositor never sees exclusive at all.
    // Hold it until the panel really becomes active, then drop straight back
    // to on demand so the panel does not keep swallowing the keyboard from
    // application windows.
    window->setProperty(PendingProperty, true);
    surface->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    window->requestActivate();

    auto *watch = new QObject(window);
    const auto release = [window, watch]() {
        if (!window->property(PendingProperty).toBool()) {
            return;
        }
        window->setProperty(PendingProperty, false);
        auto *current = LayerShellQt::Window::get(window);
        if (current != nullptr) {
            current->setKeyboardInteractivity(
                LayerShellQt::Window::KeyboardInteractivityOnDemand);
        }
        watch->deleteLater();
    };

    QObject::connect(window, &QWindow::activeChanged, watch, [window, release]() {
        if (window->isActive()) {
            release();
        }
    });

    // If focus never arrives, do not leave the panel holding the keyboard.
    QTimer::singleShot(400, watch, release);
    return true;
}
