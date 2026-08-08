#include "volumecatalog.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <QStorageInfo>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace {

bool isPseudoVolume(const QString &path, const QByteArray &fileSystem)
{
    static const QSet<QByteArray> pseudoFileSystems{
        QByteArrayLiteral("devfs"),
        QByteArrayLiteral("fdescfs"),
        QByteArrayLiteral("linprocfs"),
        QByteArrayLiteral("linsysfs"),
        QByteArrayLiteral("procfs"),
        QByteArrayLiteral("tmpfs")
    };
    if (pseudoFileSystems.contains(fileSystem)) {
        return true;
    }

    return path == QStringLiteral("/dev")
        || path.startsWith(QStringLiteral("/dev/"))
        || path == QStringLiteral("/proc")
        || path.startsWith(QStringLiteral("/proc/"))
        || path == QStringLiteral("/sys")
        || path.startsWith(QStringLiteral("/sys/"));
}

QString formatBytes(qint64 bytes)
{
    if (bytes < 0) {
        return QStringLiteral("Unknown");
    }
    if (bytes >= 1024LL * 1024LL * 1024LL) {
        return QLocale().toString(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 'f', 1)
            + QStringLiteral(" GiB");
    }
    if (bytes >= 1024LL * 1024LL) {
        return QLocale().toString(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 1)
            + QStringLiteral(" MiB");
    }
    return QLocale().formattedDataSize(bytes);
}

} // namespace

VolumeController::VolumeController(QObject *parent)
    : QObject(parent)
{
    refresh();
}

QList<VolumeEntry> VolumeController::entries() const
{
    return m_entries;
}

QVariantList VolumeController::volumes() const
{
    return toVariantList(m_entries);
}

QVariantList VolumeController::toVariantList(const QList<VolumeEntry> &entries)
{
    QVariantList result;
    result.reserve(entries.size());

    for (const VolumeEntry &entry : entries) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), entry.id);
        item.insert(QStringLiteral("name"), entry.name);
        item.insert(QStringLiteral("path"), entry.path);
        item.insert(QStringLiteral("fileSystem"), entry.fileSystem);
        item.insert(QStringLiteral("totalBytes"), entry.totalBytes);
        item.insert(QStringLiteral("availableBytes"), entry.availableBytes);
        item.insert(QStringLiteral("readOnly"), entry.readOnly);
        item.insert(QStringLiteral("isSystem"), entry.system);
        item.insert(QStringLiteral("capacityLabel"), entry.availableBytes >= 0
                ? formatBytes(entry.availableBytes) + QStringLiteral(" free")
                : QStringLiteral("Capacity unavailable"));
        result.append(item);
    }

    return result;
}

bool VolumeController::refresh()
{
    QList<VolumeEntry> discovered;
    QSet<QString> seenPaths;

    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) {
            continue;
        }

        const QString path = QDir::cleanPath(storage.rootPath());
        if (path.isEmpty() || isPseudoVolume(path, storage.fileSystemType()) || seenPaths.contains(path)) {
            continue;
        }
        seenPaths.insert(path);

        QString name = storage.displayName().trimmed();
        if (name.isEmpty()) {
            name = path == QStringLiteral("/")
                ? QStringLiteral("System")
                : QFileInfo(path).fileName();
        }
        if (name.isEmpty()) {
            name = path;
        }

        VolumeEntry entry;
        entry.id = QStringLiteral("volume:") + path;
        entry.name = name;
        entry.path = path;
        entry.fileSystem = QString::fromLocal8Bit(storage.fileSystemType());
        entry.totalBytes = storage.bytesTotal();
        entry.availableBytes = storage.bytesAvailable();
        entry.readOnly = storage.isReadOnly();
        entry.system = path == QStringLiteral("/");
        discovered.append(std::move(entry));
    }

    std::sort(discovered.begin(), discovered.end(), [](const VolumeEntry &left, const VolumeEntry &right) {
        if (left.system != right.system) {
            return left.system;
        }
        const int nameComparison = QString::compare(left.name, right.name, Qt::CaseInsensitive);
        if (nameComparison != 0) {
            return nameComparison < 0;
        }
        return QString::compare(left.path, right.path, Qt::CaseInsensitive) < 0;
    });

    if (discovered == m_entries) {
        return false;
    }

    m_entries = std::move(discovered);
    emit volumesChanged();
    return true;
}
