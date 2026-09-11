#include "storageaccess.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTextStream>
#ifdef Q_OS_FREEBSD
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#endif

namespace {
bool run(const QString &program, const QStringList &args, QByteArray *out, int timeout = 5000)
{
    QProcess p;
    QProcessEnvironment env;
    env.insert("PATH", "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin");
    env.insert("LC_ALL", "C");
    p.setProcessEnvironment(env);
    p.start(program, args);
    if (!p.waitForFinished(timeout)) { p.kill(); p.waitForFinished(2000); return false; }
    *out = p.readAllStandardOutput();
    return out->size() <= 1024 * 1024 && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}
#ifdef Q_OS_FREEBSD
bool directory(const QString &path)
{
    struct stat s {};
    const auto bytes = QFile::encodeName(path);
    if (::lstat(bytes.constData(), &s) != 0) {
        if (::mkdir(bytes.constData(), 0755) != 0 || ::lstat(bytes.constData(), &s) != 0) return false;
    }
    return S_ISDIR(s.st_mode) && s.st_uid == 0 && !(s.st_mode & 0022);
}
bool nativeMount(const QString &target, const QString &device, bool *mounted)
{
    *mounted = false;
    struct statfs *entries = nullptr;
    const int count = ::getmntinfo(&entries, MNT_NOWAIT);
    if (count <= 0) return false;
    struct stat dev {};
    if (::stat(QFile::encodeName(device).constData(), &dev) != 0) return false;
    for (int i = 0; i < count; ++i) {
        const QString destination = QString::fromLocal8Bit(entries[i].f_mntonname);
        struct stat source {};
        const bool same = ::stat(entries[i].f_mntfromname, &source) == 0
            && S_ISCHR(source.st_mode) && source.st_rdev == dev.st_rdev;
        if (destination == target) {
            if (!same || (entries[i].f_flags & (MNT_RDONLY | MNT_NOSUID | MNT_NOEXEC))
                    != (MNT_RDONLY | MNT_NOSUID | MNT_NOEXEC)) return false;
            *mounted = true;
        } else if (same) return false; // Never adopt or unmount an existing external mount.
    }
    return true;
}
#endif
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream output(stdout), error(stderr);
    const auto fail = [&error](const QString &message) { error << message << '\n'; return 1; };
#ifndef Q_OS_FREEBSD
    return fail("This helper requires FreeBSD.");
#else
    const auto args = app.arguments();
    bool uidOk = false;
    const uint uid = qEnvironmentVariable("PKEXEC_UID").toUInt(&uidOk);
    if (::geteuid() != 0 || !uidOk || uid < 1000 || !::getpwuid(uid))
        return fail("Administrator authorization for a regular desktop user is required.");
    if (args.size() != 4 || (args[1] != "--mount-readonly" && args[1] != "--unmount")
        || !QRegularExpression("^[a-f0-9]{64}$").match(args[3]).hasMatch())
        return fail("Invalid storage request.");
    const QString device = args[2], destination = StorageAccess::mountPath(uid, device);
    if (destination.isEmpty()) return fail("Unsupported device name.");
    const int lock = ::open("/var/run/northstar-storage.lock", O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    struct stat lockStat {};
    if (lock < 0 || ::fstat(lock, &lockStat) || !S_ISREG(lockStat.st_mode)
        || lockStat.st_uid != 0 || lockStat.st_nlink != 1 || (lockStat.st_mode & 0077)
        || ::flock(lock, LOCK_EX | LOCK_NB)) return fail("Another storage operation is active or the lock is unsafe.");
    const auto snapshot = StorageAccess::inspect();
    if (!snapshot.error.isEmpty()) return fail(snapshot.error);
    QVariantMap selected;
    for (const auto &entry : StorageAccess::describe(snapshot.objects)) {
        const auto candidate = entry.toMap();
        if (candidate.value("device") == device && candidate.value("identity") == args[3]) selected = candidate;
    }
    if (selected.isEmpty() || !selected.value("eligible").toBool())
        return fail("The device changed or is not an eligible USB data partition. Refresh Devices.");
    const QString disk = device.mid(5).section('p', 0, 0);
    QString serial, uuid;
    for (auto it = snapshot.objects.cbegin(); it != snapshot.objects.cend(); ++it) {
        const auto drive = it.value().value("org.freedesktop.UDisks2.Drive");
        if (it.key().path() == selected.value("drive").toString()) serial = drive.value("Serial").toString();
        QByteArray bytes = it.value().value("org.freedesktop.UDisks2.Block").value("Device").toByteArray();
        if (bytes.endsWith('\0')) bytes.chop(1);
        if (bytes == device.toUtf8()) uuid = it.value().value("org.freedesktop.UDisks2.Partition").value("UUID").toString();
    }
    QByteArray disks, parts, flags;
    if (!run("/sbin/geom", {"disk", "list", disk}, &disks)
        || !run("/sbin/geom", {"part", "list", disk}, &parts)
        || !run("/sbin/sysctl", {"-n", "kern.cam.da." + disk.mid(2) + ".flags"}, &flags)
        || !flags.contains("PACK_REMOVABLE")) return fail("Fresh kernel device verification failed.");
    bool serialMatches = false, uuidMatches = false, inSelected = false;
    for (const auto &line : disks.split('\n')) if (line.trimmed() == "ident: " + serial.toUtf8()) serialMatches = true;
    for (const auto &line : parts.split('\n')) {
        const auto trimmed = line.trimmed();
        if (trimmed.contains(". Name: ")) inSelected = trimmed.mid(trimmed.indexOf("Name: ") + 6) == device.mid(5).toUtf8();
        if (inSelected && trimmed == "rawuuid: " + uuid.toUtf8()) uuidMatches = true;
        if (trimmed.startsWith("type: ") && trimmed != "type: ms-basic-data")
            return fail("The drive contains protected or unknown partitions.");
        if (args[1] == "--mount-readonly" && trimmed.startsWith("Mode: ") && trimmed != "Mode: r0w0e0")
            return fail("The drive is in use. Close other storage activity before mounting.");
    }
    if (!serialMatches || !uuidMatches) return fail("Kernel and service device identities disagree. Refresh Devices.");
    bool mounted = false;
    if (!nativeMount(destination, device, &mounted)) return fail("Existing mount state is not safe for this request.");
    if (!directory("/media") || !directory("/media/northstar")) return fail("The controlled mount directory is unsafe.");
    QByteArray ignored;
    if (args[1] == "--unmount") {
        if (!mounted) return fail("This partition is not mounted by Northstar for your account.");
        const bool ok = run("/sbin/umount", {destination}, &ignored, 15000);
        if (!nativeMount(destination, device, &mounted) || mounted || !ok)
            return fail("Unmount failed or the volume is busy. Close files and try again; no force was used.");
        ::rmdir(QFile::encodeName(destination).constData());
        output << "Volume unmounted. Other partitions must also be unmounted before unplugging.\n";
        return 0;
    }
    if (mounted) return fail("The volume is already mounted.");
    if (!directory(destination)) return fail("Mount destination is unsafe.");
    if (!QDir(destination).isEmpty(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
        return fail("Mount destination is not empty.");
    QByteArray type;
    if (!run("/usr/sbin/fstyp", {device}, &type) || type.trimmed() != "ntfs")
        return fail("Fresh filesystem check did not confirm NTFS.");
    const QString options = QStringLiteral("ro,norecover,nosuid,noexec,uid=%1").arg(uid);
    const bool ok = run("/usr/local/bin/ntfs-3g", {"-o", options, device, destination}, &ignored, 15000);
    if (!nativeMount(destination, device, &mounted) || !mounted || !ok)
        return fail("Mount was not confirmed read-only. Do not retry blindly; inspect the mount state.");
    output << "Mounted read-only at " << destination << ". Writing is disabled.\n";
    return 0;
#endif
}
