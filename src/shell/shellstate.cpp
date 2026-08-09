#include "shellstate.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

QString defaultSettingsPath()
{
    QString configDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDirectory.isEmpty()) {
        configDirectory = QDir::home().filePath(QStringLiteral(".config/northstar"));
    }
    return QDir(configDirectory).filePath(QStringLiteral("preferences.ini"));
}

} // namespace

ShellState::ShellState(QObject *parent, QString settingsPath)
    : QObject(parent)
    , m_pinnedApplications({QStringLiteral("qterminal"), QStringLiteral("firefox")})
    , m_settingsPath(settingsPath.trimmed().isEmpty()
            ? defaultSettingsPath()
            : QDir::cleanPath(QDir::fromNativeSeparators(settingsPath)))
    , m_activeWindowTitle(QStringLiteral("Desktop"))
{
    loadPreferences();
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

bool ShellState::filesGridView() const
{
    return m_filesGridView;
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
    savePreferences();
    emit darkModeChanged();
}

void ShellState::setFilesGridView(bool enabled)
{
    if (m_filesGridView == enabled) {
        return;
    }

    m_filesGridView = enabled;
    savePreferences();
    emit filesGridViewChanged();
}

void ShellState::toggleDarkMode()
{
    setDarkMode(!m_darkMode);
}

void ShellState::loadPreferences()
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    if (settings.contains(QStringLiteral("appearance/darkMode"))) {
        m_darkMode = settings.value(QStringLiteral("appearance/darkMode"), true).toBool();
    }
    if (settings.contains(QStringLiteral("files/gridView"))) {
        m_filesGridView = settings.value(QStringLiteral("files/gridView"), true).toBool();
    }
}

void ShellState::savePreferences() const
{
    const QFileInfo settingsInfo(m_settingsPath);
    if (!QDir().mkpath(settingsInfo.absolutePath())) {
        return;
    }

    QSettings settings(m_settingsPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/darkMode"), m_darkMode);
    settings.setValue(QStringLiteral("files/gridView"), m_filesGridView);
    settings.sync();
}
