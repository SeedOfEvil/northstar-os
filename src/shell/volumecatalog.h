#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QFutureWatcher>
#include <QProcess>
#include "removablestorage.h"

struct VolumeEntry
{
    QString id;
    QString name;
    QString path;
    QString fileSystem;
    qint64 totalBytes = 0;
    qint64 availableBytes = 0;
    bool readOnly = false;
    bool system = false;
};

inline bool operator==(const VolumeEntry &left, const VolumeEntry &right)
{
    return left.id == right.id
        && left.name == right.name
        && left.path == right.path
        && left.fileSystem == right.fileSystem
        && left.totalBytes == right.totalBytes
        && left.availableBytes == right.availableBytes
        && left.readOnly == right.readOnly
        && left.system == right.system;
}

class VolumeController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList volumes READ volumes NOTIFY volumesChanged)
    Q_PROPERTY(QVariantList removableDevices READ removableDevices NOTIFY removableChanged)
    Q_PROPERTY(QString removableStatus READ removableStatus NOTIFY removableChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY removableChanged)
    Q_PROPERTY(QVariantList storagePartitions READ storagePartitions NOTIFY removableChanged)
    Q_PROPERTY(QString operationStatus READ operationStatus NOTIFY storageOperationChanged)
    Q_PROPERTY(bool operationBusy READ operationBusy NOTIFY storageOperationChanged)

public:
    explicit VolumeController(QObject *parent = nullptr);

    QList<VolumeEntry> entries() const;
    QVariantList volumes() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE void scanRemovable();
    QVariantList removableDevices() const { return m_removable; }
    QString removableStatus() const { return m_removableStatus; }
    bool scanning() const { return m_scanning; }
    QVariantList storagePartitions() const { return m_partitions; }
    QString operationStatus() const { return m_operationStatus; }
    bool operationBusy() const { return m_action.state() != QProcess::NotRunning; }
    Q_INVOKABLE void storageAction(const QString &device, const QString &identity, bool mount);

signals:
    void volumesChanged();
    void removableChanged();
    void removableScanFinished();
    void storageOperationChanged();
    void storageOperationFinished();
    void mountedLocationUnavailable(const QString &path);

private:
    static QVariantList toVariantList(const QList<VolumeEntry> &entries);

    QList<VolumeEntry> m_entries;
    QVariantList m_removable;
    QString m_removableStatus;
    QFutureWatcher<RemovableStorage::Scan> m_scan;
    bool m_scanning = false;
    QVariantList m_partitions;
    QProcess m_action;
    QByteArray m_actionOutput;
    QString m_operationStatus;
};
