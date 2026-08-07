#include "shellstate.h"

ShellState::ShellState(QObject *parent)
    : QObject(parent)
    , m_pinnedApplications({QStringLiteral("qterminal"), QStringLiteral("firefox")})
    , m_activeWindowTitle(QStringLiteral("Desktop"))
{
}

QStringList ShellState::pinnedApplications() const
{
    return m_pinnedApplications;
}

QString ShellState::activeWindowTitle() const
{
    return m_activeWindowTitle;
}

bool ShellState::darkMode() const
{
    return m_darkMode;
}

void ShellState::setActiveWindowTitle(const QString &title)
{
    const QString normalized = title.trimmed().isEmpty() ? QStringLiteral("Desktop") : title.trimmed();
    if (m_activeWindowTitle == normalized) {
        return;
    }

    m_activeWindowTitle = normalized;
    emit activeWindowTitleChanged();
}

void ShellState::setDarkMode(bool enabled)
{
    if (m_darkMode == enabled) {
        return;
    }

    m_darkMode = enabled;
    emit darkModeChanged();
}

void ShellState::toggleDarkMode()
{
    setDarkMode(!m_darkMode);
}
