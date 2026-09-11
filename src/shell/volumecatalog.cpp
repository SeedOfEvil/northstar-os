#include "volumecatalog.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <QStorageInfo>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>
#include "storageaccess.h"
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

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
    connect(&m_scan, &QFutureWatcher<RemovableStorage::Scan>::finished, this, [this] {
        const auto result = m_scan.result();
        m_removable = result.devices;
        m_removableStatus = result.status;
        m_partitions = result.partitions;
        emit removableChanged();
    });
    m_action.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_action, &QProcess::readyReadStandardOutput, this, [this] {
        m_actionOutput += m_action.readAllStandardOutput();
        if (m_actionOutput.size() > 16384) m_actionOutput = m_actionOutput.right(16384);
    });
    connect(&m_action, &QProcess::finished, this, [this](int code, QProcess::ExitStatus state) {
        m_actionOutput += m_action.readAllStandardOutput();
        m_operationStatus = QString::fromUtf8(m_actionOutput.right(16384)).trimmed();
        if (m_operationStatus.isEmpty()) m_operationStatus = code == 0 && state == QProcess::NormalExit
            ? QStringLiteral("Operation completed. Refresh to verify state.") : QStringLiteral("Operation failed or authorization was cancelled.");
        emit storageOperationChanged();
        scanRemovable();
        refresh();
        emit storageOperationFinished();
    });
    connect(&m_action, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_operationStatus = QStringLiteral("Could not start the protected storage helper. Check installation.");
        emit storageOperationChanged();
        emit storageOperationFinished();
    });
    refresh();
}

void VolumeController::scanRemovable()
{
    if (m_scan.isRunning() || operationBusy()) return;
    m_removable.clear();
    m_partitions.clear();
    m_removableStatus = QStringLiteral("Scanning removable-device metadata...");
    m_scan.setFuture(QtConcurrent::run([] {
        auto result = RemovableStorage::scan();
        const auto snapshot = StorageAccess::inspect();
        if (!snapshot.error.isEmpty()) { result.status = snapshot.error; return result; }
        result.partitions = StorageAccess::describe(snapshot.objects);
#ifdef Q_OS_UNIX
        for (auto &entry : result.partitions) {
            auto row = entry.toMap();
            const QString target = StorageAccess::mountPath(::getuid(), row.value("device").toString());
            const QStorageInfo storage(target);
            row.insert("mounted", !target.isEmpty() && storage.isReady() && storage.rootPath() == target);
            row.insert("mountPath", target);
            entry = row;
        }
#endif
        result.status = QStringLiteral("NTFS data partitions can be mounted read-only. Authorization is required. Boot/helper partitions remain protected.");
        return result;
    }));
    emit removableChanged();
}

void VolumeController::storageAction(const QString &device, const QString &identity, bool mount)
{
    if (operationBusy() || scanning()) return;
    bool allowed = false;
    for (const auto &entry : m_partitions) {
        const auto row = entry.toMap();
        if (row.value("device") == device && row.value("identity") == identity
            && row.value("eligible").toBool() && row.value("mounted").toBool() != mount) allowed = true;
    }
    if (!allowed) {
        m_operationStatus = QStringLiteral("Selection is stale or protected. Refresh Devices.");
        emit storageOperationChanged(); emit storageOperationFinished(); return;
    }
    m_actionOutput.clear();
    m_operationStatus = QStringLiteral("Waiting for administrator authorization and storage verification...");
    m_action.start(QStringLiteral("/usr/local/bin/pkexec"),
        {QStringLiteral("/usr/local/libexec/northstar-storage"), mount ? QStringLiteral("--mount-readonly") : QStringLiteral("--unmount"), device, identity});
    emit storageOperationChanged();
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
