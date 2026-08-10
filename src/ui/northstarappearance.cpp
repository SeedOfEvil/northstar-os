#include "northstarappearance.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

bool NorthstarAppearance::darkMode()
{
    QString configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (configRoot.isEmpty()) {
        configRoot = QDir::home().filePath(QStringLiteral(".config"));
    }
    const QString settingsPath = QDir(configRoot).filePath(
        QStringLiteral("northstar-shell/preferences.ini"));
    QSettings settings(settingsPath, QSettings::IniFormat);
    return settings.value(QStringLiteral("appearance/darkMode"), true).toBool();
}
