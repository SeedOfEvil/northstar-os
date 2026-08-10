#include "pinnedapplicationmodel.h"

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

const QStringList DefaultPins{
    QStringLiteral("qterminal"),
    QStringLiteral("firefox"),
};

} // namespace

PinnedApplicationModel::PinnedApplicationModel(QObject *parent, QString settingsPath)
    : QAbstractListModel(parent)
    , m_settingsPath(settingsPath.trimmed().isEmpty()
            ? defaultSettingsPath()
            : QDir::cleanPath(QDir::fromNativeSeparators(settingsPath)))
{
    load();
}

int PinnedApplicationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_desktopIds.size();
}

int PinnedApplicationModel::count() const
{
    return m_desktopIds.size();
}

QVariant PinnedApplicationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_desktopIds.size()) {
        return {};
    }
    if (role == DesktopIdRole || role == Qt::DisplayRole) {
        return m_desktopIds.at(index.row());
    }
    return {};
}

QHash<int, QByteArray> PinnedApplicationModel::roleNames() const
{
    return {{DesktopIdRole, QByteArrayLiteral("desktopId")}};
}

QStringList PinnedApplicationModel::desktopIds() const
{
    return m_desktopIds;
}

bool PinnedApplicationModel::isPinned(const QString &desktopId) const
{
    return m_desktopIds.contains(normalizedDesktopId(desktopId));
}

bool PinnedApplicationModel::pin(const QString &desktopId)
{
    const QString normalized = normalizedDesktopId(desktopId);
    if (normalized.isEmpty() || m_desktopIds.contains(normalized)) {
        return false;
    }

    const int row = m_desktopIds.size();
    beginInsertRows({}, row, row);
    m_desktopIds.append(normalized);
    endInsertRows();
    save();
    emit desktopIdsChanged();
    return true;
}

bool PinnedApplicationModel::unpin(const QString &desktopId)
{
    const int row = m_desktopIds.indexOf(normalizedDesktopId(desktopId));
    if (row < 0) {
        return false;
    }

    beginRemoveRows({}, row, row);
    m_desktopIds.removeAt(row);
    endRemoveRows();
    save();
    emit desktopIdsChanged();
    return true;
}

bool PinnedApplicationModel::movePinned(int from, int to)
{
    if (from < 0 || from >= m_desktopIds.size() || to < 0 || to >= m_desktopIds.size()
        || from == to) {
        return false;
    }

    const int destination = to > from ? to + 1 : to;
    if (!beginMoveRows({}, from, from, {}, destination)) {
        return false;
    }
    m_desktopIds.move(from, to);
    endMoveRows();
    save();
    emit desktopIdsChanged();
    return true;
}

QString PinnedApplicationModel::normalizedDesktopId(const QString &desktopId)
{
    const QString normalized = desktopId.trimmed();
    if (normalized.isEmpty() || normalized.size() > 256
        || normalized.contains(QLatin1Char('\n')) || normalized.contains(QLatin1Char('\r'))) {
        return {};
    }
    return normalized;
}

void PinnedApplicationModel::load()
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    const QStringList stored = settings.value(QStringLiteral("dock/pinnedApplications"), DefaultPins).toStringList();
    for (const QString &desktopId : stored) {
        const QString normalized = normalizedDesktopId(desktopId);
        if (!normalized.isEmpty() && !m_desktopIds.contains(normalized)) {
            m_desktopIds.append(normalized);
        }
    }
}

void PinnedApplicationModel::save() const
{
    const QFileInfo settingsInfo(m_settingsPath);
    if (!QDir().mkpath(settingsInfo.absolutePath())) {
        return;
    }
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("dock/pinnedApplications"), m_desktopIds);
    settings.sync();
}
