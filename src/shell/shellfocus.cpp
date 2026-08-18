#include "shellfocus.h"

#include "layershellsurface.h"

#include <QWindow>

ShellFocus::ShellFocus(QObject *parent)
    : QObject(parent)
{
}

void ShellFocus::setPanelWindow(QWindow *window)
{
    m_panel = window;
}

bool ShellFocus::restore()
{
    if (m_panel.isNull()) {
        return false;
    }
    return LayerShellSurface::restoreKeyboardFocus(m_panel);
}
