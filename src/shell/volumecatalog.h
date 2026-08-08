#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

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

public:
    explicit VolumeController(QObject *parent = nullptr);

    QList<VolumeEntry> entries() const;
    QVariantList volumes() const;

    Q_INVOKABLE bool refresh();

signals:
    void volumesChanged();

private:
    static QVariantList toVariantList(const QList<VolumeEntry> &entries);

    QList<VolumeEntry> m_entries;
};
