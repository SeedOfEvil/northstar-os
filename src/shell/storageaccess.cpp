#include "storageaccess.h"
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QRegularExpression>

namespace {
QString path(const QVariant &value) { return qvariant_cast<QDBusObjectPath>(value).path(); }
QString deviceName(const QVariant &value) {
    QByteArray bytes = value.toByteArray();
    if (bytes.endsWith('\0')) bytes.chop(1);
    return QString::fromUtf8(bytes);
}
QVariantMap props(const StorageAccess::Interfaces &interfaces, const char *type) {
    return interfaces.value(QStringLiteral("org.freedesktop.UDisks2.") + QLatin1String(type));
}
}

StorageAccess::Snapshot StorageAccess::inspect()
{
    qDBusRegisterMetaType<Interfaces>();
    qDBusRegisterMetaType<Objects>();
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.UDisks2"),
        QStringLiteral("/org/freedesktop/UDisks2"), QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("GetManagedObjects"));
    QDBusReply<Objects> reply = QDBusConnection::systemBus().call(message, QDBus::Block, 5000);
    if (!reply.isValid()) return {{}, QStringLiteral("Storage service unavailable. Check that bsdisks is installed.")};
    if (reply.value().size() > 512) return {{}, QStringLiteral("Storage inventory exceeds the supported limit.")};
    return {reply.value(), {}};
}

QString StorageAccess::mountPath(uint uid, const QString &device)
{
    if (!QRegularExpression(QStringLiteral("^/dev/da[0-9]{1,5}p[0-9]{1,3}$")).match(device).hasMatch())
        return {};
    return QStringLiteral("/media/northstar/%1-%2").arg(uid).arg(device.mid(5));
}

QVariantList StorageAccess::describe(const Objects &objects)
{
    QVariantList result;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const auto block = props(it.value(), "Block");
        const QString drivePath = path(block.value("Drive"));
        const auto drive = props(objects.value(QDBusObjectPath(drivePath.isEmpty() ? "/" : drivePath)), "Drive");
        if (drive.value("ConnectionBus").toString() != "usb" || !drive.value("Removable").toBool()) continue;
        const auto partition = props(it.value(), "Partition");
        if (partition.isEmpty()) continue;
        const QString device = deviceName(block.value("Device"));
        QString reason;
        if (mountPath(1000, device).isEmpty()) reason = "Unsupported partition layout.";
        else if (!it.value().contains("org.freedesktop.UDisks2.Filesystem")
                 || block.value("IdUsage").toString() != "filesystem") reason = "Not a supported filesystem.";
        else if (block.value("IdType").toString() != "ntfs") reason = "This first mount slice supports NTFS only.";
        else if (block.value("HintIgnore").toBool() || block.value("HintSystem").toBool()
                 || partition.value("Type").toString() != "ms-basic-data"
                 || block.value("Size").toULongLong() < 16 * 1024 * 1024
                 || block.value("IdLabel").toString().startsWith("UEFI", Qt::CaseInsensitive))
            reason = "Boot/helper or protected partition; no actions allowed.";
        // A removable transport is not enough: refuse disks containing OS/pool/swap partitions.
        for (auto sibling = objects.cbegin(); sibling != objects.cend(); ++sibling) {
            const auto other = props(sibling.value(), "Block");
            if (path(other.value("Drive")) != drivePath) continue;
            const auto otherPart = props(sibling.value(), "Partition");
            if (!otherPart.isEmpty() && (otherPart.value("Type").toString() != "ms-basic-data"
                || other.value("HintIgnore").toBool() || other.value("HintSystem").toBool()
                || !sibling.value().contains("org.freedesktop.UDisks2.Filesystem")))
                reason = "The drive contains system, unknown or protected partitions.";
        }
        QByteArray identity;
        for (const QString &s : {drivePath, drive.value("Serial").toString(),
                device, block.value("DeviceNumber").toString(), partition.value("UUID").toString(),
                block.value("Size").toString()}) {
            identity += s.toUtf8(); identity += '\0';
        }
        if (drive.value("Serial").toString().isEmpty() || partition.value("UUID").toString().isEmpty())
            reason = "Stable drive and partition identity unavailable.";
        result.append(QVariantMap{{"device", device}, {"name", block.value("IdLabel").toString().left(160)},
            {"fileSystem", block.value("IdType")}, {"drive", drivePath}, {"totalBytes", block.value("Size")},
            {"identity", QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex())},
            {"eligible", reason.isEmpty()}, {"reason", reason}});
    }
    return result;
}
