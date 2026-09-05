#include "applicationcompatibility.h"
#include "applicationbundlecatalog.h"
#include "webapplication.h"

#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
QVariantMap result(const QString &status, const QString &format, const QString &message)
{
    return {{QStringLiteral("status"), status}, {QStringLiteral("format"), format},
            {QStringLiteral("message"), message}, {QStringLiteral("runtimeVerified"), false}};
}

QVariantMap binaryReport(const QString &path)
{
    QFile file(path);
#ifdef Q_OS_UNIX
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    struct stat info {};
    if (fd < 0)
        return result("unverified", "Unreadable", "Cannot read the application file. Check its path and read permissions.");
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)
        || !file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return result("unverified", "Unreadable", "Only regular files can be inspected; links and devices are not supported.");
    }
#else
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || !file.open(QIODevice::ReadOnly))
        return result("unverified", "Unreadable", "Select a readable regular file, not a link or device.");
#endif
    // Bounded header inspection only. Never invoke the file, its interpreter or ldd.
    const QByteArray data = file.read(4096);
    const auto byte = [&data](int index) { return static_cast<unsigned char>(data.at(index)); };
    if (data.startsWith(QByteArray("\177ELF", 4))) {
        if (data.size() < 52 || (byte(4) != 1 && byte(4) != 2)
            || (byte(5) != 1 && byte(5) != 2) || byte(6) != 1
            || (byte(4) == 2 && data.size() < 64))
            return result("unverified", "ELF", "The ELF header is incomplete or invalid. Obtain a fresh application build.");
        const quint16 machine = byte(5) == 1 ? qFromLittleEndian<quint16>(data.constData() + 18)
                                             : qFromBigEndian<quint16>(data.constData() + 18);
        const QString abi = byte(7) == 9 ? QStringLiteral("FreeBSD")
            : byte(7) == 3 ? QStringLiteral("Linux") : QStringLiteral("unspecified/other ABI");
        return result("unverified", QStringLiteral("ELF %1-bit • %2 • machine %3")
            .arg(byte(4) == 2 ? 64 : 32).arg(abi).arg(machine),
            "ELF format recognized. Architecture, operating-system compatibility, libraries and runtime behavior are not verified. Request a Northstar/FreeBSD build for this machine; Linux ELF is not native compatibility evidence.");
    }
    if (data.startsWith("#!/"))
        return result("unverified", "Interpreter script", "Script format recognized. Its interpreter, dependencies and behavior are not verified. Review the source and required runtime before launching.");
    if (data.startsWith("MZ") && data.size() >= 64) {
        const quint32 offset = qFromLittleEndian<quint32>(data.constData() + 60);
        if (offset >= 64 && offset <= 1024 * 1024 && file.seek(offset)
            && file.read(4) == QByteArray("PE\0\0", 4))
            return result("unsupported", "PE (Windows-family)", "Not a native Northstar application. A Windows compatibility runtime is not provided by this installer. Ask the publisher for a Northstar/FreeBSD or web version.");
    }
    if (data.size() >= 28) {
        const QByteArray magic = data.left(4).toHex();
        if (magic == "feedface" || magic == "cefaedfe" || magic == "feedfacf" || magic == "cffaedfe")
            return result("unsupported", "Mach-O (Apple-family)", "Not a native Northstar application. macOS frameworks and runtime are not provided by this installer. Ask the publisher for a Northstar/FreeBSD or web version.");
    }
    return result("unverified", "Unknown format", "No supported header was identified. File names and .app extensions do not establish compatibility. Universal Apple binaries and other containers are not classified by this report.");
}
}

QVariantMap ApplicationCompatibility::report(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isDir() && !info.fileName().endsWith(QStringLiteral(".app")))
        return binaryReport(path);
    BundleApplication bundle;
    QString reason;
    if (!ApplicationBundleCatalog::inspectBundle(path, &bundle, &reason))
        return result("invalid-bundle", "Invalid Northstar bundle", reason);
    if (!bundle.webUrl.isEmpty())
        return result("web", "Web application", WebApplication::notice()
            + QStringLiteral(" Firefox availability, internet access and website behavior have not been tested by this report."));
    return binaryReport(bundle.executablePath);
}
