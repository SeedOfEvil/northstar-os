#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>

struct InstallerDisk
{
    QString device;
    quint64 sizeBytes = 0;
    quint32 sectorSize = 0;
    QString sizeText;
    QString description;
    QString transport;
    bool systemDisk = false;
    bool eligible = false;
    QString reason;
};

class InstallerDiskModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        DeviceRole = Qt::UserRole + 1,
        SizeBytesRole,
        SectorSizeRole,
        SizeTextRole,
        DescriptionRole,
        TransportRole,
        SystemDiskRole,
        EligibleRole,
        ReasonRole,
    };
    Q_ENUM(Role)

    explicit InstallerDiskModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const InstallerDisk *diskAt(int row) const;
    void replace(QList<InstallerDisk> disks);
    void clear();

signals:
    void countChanged();

private:
    QList<InstallerDisk> m_disks;
};
