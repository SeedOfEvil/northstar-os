#include "desktoplayoutcontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace {

QString defaultSettingsPath()
{
    QString configDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDirectory.isEmpty()) {
        configDirectory = QDir::home().filePath(QStringLiteral(".config/northstar"));
    }
    return QDir(configDirectory).filePath(QStringLiteral("desktop-layout.ini"));
}

qreal boundedCoordinate(qreal coordinate)
{
    if (!std::isfinite(coordinate)) {
        return 0.0;
    }
    return std::max<qreal>(0.0, std::min<qreal>(4096.0, coordinate));
}

} // namespace

DesktopLayoutController::DesktopLayoutController(QObject *parent, QString settingsPath)
    : QObject(parent)
    , m_settingsPath(settingsPath.trimmed().isEmpty()
            ? defaultSettingsPath()
            : QDir::cleanPath(QDir::fromNativeSeparators(settingsPath)))
{
    loadPreferences();
}

QVariantMap DesktopLayoutController::positions() const
{
    return m_positions;
}

QVariantMap DesktopLayoutController::positionFor(const QString &path) const
{
    return m_positions.value(normalizedPath(path)).toMap();
}

bool DesktopLayoutController::setPosition(const QString &path, qreal x, qreal y)
{
    const QString normalized = normalizedPath(path);
    if (normalized.isEmpty()) {
        return false;
    }

    const QVariantMap nextPosition{
        {QStringLiteral("x"), boundedCoordinate(x)},
        {QStringLiteral("y"), boundedCoordinate(y)},
    };
    if (m_positions.value(normalized).toMap() == nextPosition) {
        return true;
    }

    m_positions.insert(normalized, nextPosition);
    if (!savePreferences()) {
        return false;
    }
    emit positionsChanged();
    return true;
}

bool DesktopLayoutController::clearPosition(const QString &path)
{
    const QString normalized = normalizedPath(path);
    if (!m_positions.contains(normalized)) {
        return true;
    }

    m_positions.remove(normalized);
    if (!savePreferences()) {
        return false;
    }
    emit positionsChanged();
    return true;
}

void DesktopLayoutController::reset()
{
    if (m_positions.isEmpty()) {
        return;
    }

    m_positions.clear();
    if (savePreferences()) {
        emit positionsChanged();
    }
}

QString DesktopLayoutController::normalizedPath(const QString &path)
{
    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    return normalized == QStringLiteral(".") ? QString() : normalized;
}

QString DesktopLayoutController::settingsKeyFor(const QString &path)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(path));
}

QString DesktopLayoutController::pathForSettingsKey(const QString &key)
{
    return QUrl::fromPercentEncoding(key.toLatin1());
}

void DesktopLayoutController::loadPreferences()
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("desktopPositions"));
    for (const QString &key : settings.childGroups()) {
        const QString path = pathForSettingsKey(key);
        if (path.isEmpty()) {
            continue;
        }

        m_positions.insert(path, QVariantMap{
            {QStringLiteral("x"), boundedCoordinate(settings.value(key + QStringLiteral("/x"), 0).toDouble())},
            {QStringLiteral("y"), boundedCoordinate(settings.value(key + QStringLiteral("/y"), 0).toDouble())},
        });
    }
    settings.endGroup();
}

bool DesktopLayoutController::savePreferences() const
{
    const QFileInfo settingsInfo(m_settingsPath);
    if (!QDir().mkpath(settingsInfo.absolutePath())) {
        return false;
    }

    QSettings settings(m_settingsPath, QSettings::IniFormat);
    settings.remove(QStringLiteral("desktopPositions"));
    settings.beginGroup(QStringLiteral("desktopPositions"));
    for (auto iterator = m_positions.cbegin(); iterator != m_positions.cend(); ++iterator) {
        const QString key = settingsKeyFor(iterator.key());
        const QVariantMap position = iterator.value().toMap();
        settings.setValue(key + QStringLiteral("/x"), position.value(QStringLiteral("x")));
        settings.setValue(key + QStringLiteral("/y"), position.value(QStringLiteral("y")));
    }
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError;
}
