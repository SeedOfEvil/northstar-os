#include "removablestorage.h"
#include <QProcess>
#include <QRegularExpression>
#include <QXmlStreamReader>

namespace {
constexpr qsizetype limit = 2 * 1024 * 1024;
bool readSysctl(const QStringList &arguments, QByteArray *output)
{
    QProcess process;
    process.start(QStringLiteral("/sbin/sysctl"), arguments);
    if (!process.waitForStarted(1000)) return false;
    // Keep the read bounded even while the child is running.
    for (int ticks = 0; ticks < 40; ++ticks) {
        process.waitForReadyRead(50);
        *output += process.readAllStandardOutput();
        process.readAllStandardError();
        if (output->size() > limit) break;
        if (process.state() == QProcess::NotRunning)
            return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }
    process.kill();
    process.waitForFinished(1000);
    return false;
}
}

QVariantList RemovableStorage::parse(const QByteArray &xml, const QSet<QString> &removable, bool *valid)
{
    *valid = false;
    QVariantList result;
    if (xml.size() > limit) return result;
    QXmlStreamReader reader(xml);
    QStringList stack;
    QString className, device, description;
    qint64 size = 0;
    QSet<QString> seen;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isDTD() || reader.isEntityReference()) return {};
        if (reader.isStartElement()) {
            const QString name = reader.name().toString();
            const QString path = stack.join('/') + '/' + name;
            if (stack.size() > 16) return {};
            if (path == "/mesh/class") className.clear();
            if (path == "/mesh/class/geom/provider") { device.clear(); description.clear(); size = 0; }
            if (path == "/mesh/class/name") className = reader.readElementText();
            else if (path == "/mesh/class/geom/provider/name") device = reader.readElementText();
            else if (path == "/mesh/class/geom/provider/mediasize") size = reader.readElementText().toLongLong();
            else if (path == "/mesh/class/geom/provider/config/descr") description = reader.readElementText();
            else stack.append(name);
        } else if (reader.isEndElement()) {
            if (stack.join('/') == "mesh/class/geom/provider" && className == "DISK"
                && removable.contains(device) && !seen.contains(device)
                && QRegularExpression(QStringLiteral("^da[0-9]{1,5}$")).match(device).hasMatch()
                && size > 0) {
                if (result.size() >= 32) return {};
                seen.insert(device);
                // Never expose serials, GEOM addresses or a capability to mutate.
                result.append(QVariantMap{{"device", device}, {"name", description.left(160)},
                    {"totalBytes", size}, {"canMount", false}, {"canEject", false}});
            }
            if (!stack.isEmpty()) stack.removeLast();
        }
    }
    *valid = !reader.hasError() && xml.trimmed().startsWith("<mesh>");
    return *valid ? result : QVariantList{};
}

RemovableStorage::Scan RemovableStorage::scan()
{
#ifdef Q_OS_FREEBSD
    QByteArray names;
    if (!readSysctl({"-n", "kern.disks"}, &names))
        return {{}, "Device detection unavailable. Could not read the disk inventory."};
    QStringList keys;
    for (const QByteArray &raw : names.simplified().split(' ')) {
        const QString name = QString::fromLatin1(raw);
        if (QRegularExpression(QStringLiteral("^da[0-9]{1,5}$")).match(name).hasMatch())
            keys.append("kern.cam.da." + name.mid(2) + ".flags");
    }
    if (keys.size() > 32) return {{}, "Device inventory exceeds the supported scan limit."};
    if (keys.isEmpty()) return {{}, "No removable media detected. Connect a drive and choose Refresh."};
    QByteArray flags, xml;
    if (!readSysctl(keys, &flags) || !readSysctl({"-b", "kern.geom.confxml"}, &xml))
        return {{}, "Device detection failed or the drive changed during scanning. Choose Refresh."};
    QSet<QString> removable;
    const QRegularExpression pattern(QStringLiteral("^kern\\.cam\\.da\\.([0-9]{1,5})\\.flags: [^<]*<([^>]*)>$"));
    for (const QByteArray &line : flags.split('\n')) {
        const auto match = pattern.match(QString::fromLatin1(line).trimmed());
        if (match.hasMatch() && match.captured(2).split(',').contains("PACK_REMOVABLE"))
            removable.insert("da" + match.captured(1));
    }
    bool valid;
    const auto devices = parse(xml, removable, &valid);
    if (!valid) return {{}, "Device detection returned invalid metadata. No actions are available."};
    return {devices, devices.isEmpty() ? "No removable media detected. Choose Refresh after connecting a drive."
        : "Detection only. Mounting, file access and safe removal are not enabled here yet. Refresh after plugging or unplugging."};
#else
    return {{}, "Removable-device detection requires the FreeBSD storage backend."};
#endif
}
