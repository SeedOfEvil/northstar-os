import QtQuick

// Keeps a window reachable.
//
// A window can end up where its own controls cannot be used: maximised so its
// controls sit at the far edge of a display that is not fully visible, moved
// off an edge, or sized for a screen that is no longer this one. Shell windows
// are not listed in the dock, so when that happens there is nothing left to
// click and the window is lost until the shell is restarted.
//
// This gives every window that can be maximised the same two ways back: a
// keyboard escape, and a geometry that is put within reach whenever the window
// is opened.
Item {
    id: recovery

    required property var window

    // The reserved strip at the top of the screen. A window placed under it
    // would have its title bar covered by the panel.
    property int panelHeight: 44
    property int desktopMargin: 24

    property int screenX: 0
    property int screenY: 0
    property int screenWidth: 1280
    property int screenHeight: 800
    property int minimumSurfaceWidth: 480
    property int minimumSurfaceHeight: 320

    // Something for Escape to do before it falls through to un-maximising,
    // such as clearing a search box the window owns.
    property bool handledLocally: false

    signal escapePressed()

    // Puts the window back where it can be operated. Returns whether anything
    // had to move, so a caller can tell a stranded window from a normal one.
    function restoreToReach() {
        if (!recovery.window) {
            return false
        }

        const previousX = recovery.window.x
        const previousY = recovery.window.y
        const previousWidth = recovery.window.width
        const previousHeight = recovery.window.height

        // The largest a window may be and still leave its own edges on the
        // screen. Never smaller than the window's own minimum, or clamping
        // would fight the minimum size and win.
        const widestAllowed = Math.max(recovery.minimumSurfaceWidth,
            recovery.screenWidth - (recovery.desktopMargin * 2))
        const tallestAllowed = Math.max(recovery.minimumSurfaceHeight,
            recovery.screenHeight - recovery.panelHeight - (recovery.desktopMargin * 2))

        recovery.window.width = Math.max(recovery.minimumSurfaceWidth,
            Math.min(recovery.window.width, widestAllowed))
        recovery.window.height = Math.max(recovery.minimumSurfaceHeight,
            Math.min(recovery.window.height, tallestAllowed))

        const leftMost = recovery.screenX
        const rightMost = recovery.screenX + recovery.screenWidth - recovery.window.width
        const topMost = recovery.screenY + recovery.panelHeight
        const bottomMost = recovery.screenY + recovery.screenHeight - recovery.window.height

        recovery.window.x =
            Math.max(leftMost, Math.min(recovery.window.x, Math.max(leftMost, rightMost)))
        recovery.window.y =
            Math.max(topMost, Math.min(recovery.window.y, Math.max(topMost, bottomMost)))

        return previousX !== recovery.window.x || previousY !== recovery.window.y
            || previousWidth !== recovery.window.width
            || previousHeight !== recovery.window.height
    }

    visible: false

    Shortcut {
        sequence: "Escape"
        enabled: !!recovery.window && recovery.window.visible

        onActivated: {
            // The window gets first refusal: a search box worth clearing is a
            // better answer to Escape than closing the whole window.
            if (recovery.handledLocally) {
                recovery.escapePressed()
                return
            }
            if (recovery.window.maximized !== undefined && recovery.window.maximized) {
                recovery.window.toggleMaximize()
                return
            }
            recovery.window.hide()
        }
    }
}
