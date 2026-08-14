#include "installerdiskmodel.h"

#include <utility>

InstallerDiskModel::InstallerDiskModel(QObject *parent) : QAbstractListModel(parent) {}

int InstallerDiskModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_disks.size();
}

int InstallerDiskModel::count() const { return m_disks.size(); }

QVariant InstallerDiskModel::data(const QModelIndex &index, int role) const
{
    const InstallerDisk *disk = diskAt(index.row());
    if (!index.isValid() || disk == nullptr) return {};
    switch (role) {
    case DeviceRole: return disk->device;
    case SizeBytesRole: return QVariant::fromValue(disk->sizeBytes);
    case SectorSizeRole: return QVariant::fromValue(disk->sectorSize);
    case SizeTextRole: return disk->sizeText;
    case DescriptionRole: return disk->description;
    case TransportRole: return disk->transport;
    case SystemDiskRole: return disk->systemDisk;
    case EligibleRole: return disk->eligible;
    case ReasonRole: return disk->reason;
    default: return {};
    }
}

QHash<int, QByteArray> InstallerDiskModel::roleNames() const
{
    return {{DeviceRole, "device"}, {SizeBytesRole, "sizeBytes"},
            {SectorSizeRole, "sectorSize"}, {SizeTextRole, "sizeText"},
            {DescriptionRole, "description"}, {TransportRole, "transport"},
            {SystemDiskRole, "systemDisk"}, {EligibleRole, "eligible"}, {ReasonRole, "reason"}};
}

const InstallerDisk *InstallerDiskModel::diskAt(int row) const
{
    return row >= 0 && row < m_disks.size() ? &m_disks.at(row) : nullptr;
}

void InstallerDiskModel::replace(QList<InstallerDisk> disks)
{
    beginResetModel();
    m_disks = std::move(disks);
    endResetModel();
    emit countChanged();
}

void InstallerDiskModel::clear() { replace({}); }
