#include "applicationbundleinstaller.h"

#include "applicationbundlecatalog.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

#include <utility>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {

constexpr qsizetype maximumEntries = 4096;
constexpr qint64 maximumBytes = 512LL * 1024LL * 1024LL;

QFileDevice::Permissions privateDirectoryPermissions()
{
    return QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
}

bool unsafePermissions(const QFileInfo &info)
{
    return info.permissions() & (QFileDevice::WriteGroup | QFileDevice::WriteOther);
}

uint processOwnerId()
{
#ifdef Q_OS_UNIX
    return geteuid();
#else
    return QFileInfo(QDir::homePath()).ownerId();
#endif
}

QString cleanAbsolutePath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

} // namespace

ApplicationBundleInstaller::ApplicationBundleInstaller(QString applicationRoot,
                                                       QString trashRoot,
                                                       QObject *parent,
                                                       QStringList systemRoots)
    : QObject(parent)
    , m_applicationRoot(applicationRoot.isEmpty() ? defaultApplicationRoot()
                                                  : cleanAbsolutePath(applicationRoot))
    , m_trashRoot(trashRoot.isEmpty() ? defaultTrashRoot() : cleanAbsolutePath(trashRoot))
    , m_systemRoots(std::move(systemRoots))
{
    if (applicationRoot.isEmpty() && m_systemRoots.isEmpty()) {
        m_systemRoots = ApplicationBundleCatalog::defaultBundleDirectories();
        m_systemRoots.removeAll(defaultApplicationRoot());
    }
}

QString ApplicationBundleInstaller::statusMessage() const
{
    return m_statusMessage;
}

bool ApplicationBundleInstaller::error() const
{
    return m_error;
}

QVariantMap ApplicationBundleInstaller::bundleDetails(const QString &sourcePath) const
{
    BundleApplication bundle;
    if (!ApplicationBundleCatalog::inspectBundle(sourcePath, &bundle)) {
        return {
            {QStringLiteral("valid"), false},
            {QStringLiteral("validationError"),
             QStringLiteral("The manifest, permissions, or required application files are invalid.")}
        };
    }

    const QFileInfo sourceInfo(bundle.bundlePath);
    QString validationError;
    if (sourceInfo.ownerId() != processOwnerId()
        || !validateTree(bundle.bundlePath, sourceInfo.ownerId(), &validationError)) {
        return {
            {QStringLiteral("valid"), false},
            {QStringLiteral("validationError"),
             validationError.isEmpty()
                 ? QStringLiteral("The application is not owned by the current user.")
                 : validationError}
        };
    }

    const QString scope = installedScope(bundle.bundleId);
    return {
        {QStringLiteral("valid"), true},
        {QStringLiteral("bundleIdentifier"), bundle.bundleId},
        {QStringLiteral("displayName"), bundle.name},
        {QStringLiteral("version"), bundle.version},
        {QStringLiteral("source"), bundle.provenance.source},
        {QStringLiteral("package"), bundle.provenance.package},
        {QStringLiteral("revision"), bundle.provenance.revision},
        {QStringLiteral("bundlePath"), bundle.bundlePath},
        {QStringLiteral("alreadyInstalled"), !scope.isEmpty()},
        {QStringLiteral("installedScope"), scope}
    };
}

QString ApplicationBundleInstaller::defaultApplicationRoot()
{
    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty()) {
        dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    }
    return QDir(dataHome).filePath(QStringLiteral("northstar/apps"));
}

QString ApplicationBundleInstaller::defaultTrashRoot()
{
    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty()) {
        dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    }
    return QDir(dataHome).filePath(QStringLiteral("Trash"));
}

bool ApplicationBundleInstaller::ensurePrivateDirectory(const QString &path)
{
    const QFileInfo existing(path);
    if (existing.exists()) {
        if (!existing.isDir() || existing.isSymLink() || existing.ownerId() != processOwnerId()
            || unsafePermissions(existing)) {
            return false;
        }
        return true;
    }

    if (!QDir().mkpath(path)) {
        return false;
    }
    return QFile::setPermissions(path, privateDirectoryPermissions());
}

bool ApplicationBundleInstaller::validateTree(const QString &path,
                                              uint ownerId,
                                              QString *reason) const
{
    const QFileInfo root(path);
    if (!root.isDir() || root.isSymLink() || root.ownerId() != ownerId || unsafePermissions(root)) {
        if (reason != nullptr) {
            *reason = QStringLiteral("The bundle root is not a private, owned directory.");
        }
        return false;
    }

    qsizetype entries = 0;
    qint64 bytes = 0;
    QDirIterator iterator(path,
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo info = iterator.fileInfo();
        ++entries;
        if (entries > maximumEntries || info.isSymLink() || info.ownerId() != ownerId
            || unsafePermissions(info) || (!info.isDir() && !info.isFile())) {
            if (reason != nullptr) {
                *reason = QStringLiteral("The bundle contains an unsafe or unsupported entry.");
            }
            return false;
        }
        if (info.isFile()) {
            bytes += info.size();
            if (bytes > maximumBytes) {
                if (reason != nullptr) {
                    *reason = QStringLiteral("The bundle exceeds the 512 MiB user-install limit.");
                }
                return false;
            }
        }
    }
    return true;
}

bool ApplicationBundleInstaller::copyTree(const QString &source,
                                          const QString &destination,
                                          uint ownerId,
                                          qsizetype *entries,
                                          qint64 *bytes,
                                          QString *reason) const
{
    if (!QDir().mkpath(destination)
        || !QFile::setPermissions(destination, privateDirectoryPermissions())) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Northstar could not create the private staging directory.");
        }
        return false;
    }

    const QFileInfoList sourceEntries = QDir(source).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::Name);
    for (const QFileInfo &entry : sourceEntries) {
        ++(*entries);
        if (*entries > maximumEntries || entry.isSymLink() || entry.ownerId() != ownerId
            || unsafePermissions(entry) || (!entry.isDir() && !entry.isFile())) {
            if (reason != nullptr) {
                *reason = QStringLiteral("The bundle changed or contains an unsafe entry.");
            }
            return false;
        }

        const QString target = QDir(destination).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyTree(entry.absoluteFilePath(), target, ownerId, entries, bytes, reason)) {
                return false;
            }
            continue;
        }
        *bytes += entry.size();
        if (*bytes > maximumBytes) {
            if (reason != nullptr) {
                *reason = QStringLiteral("The bundle exceeds the 512 MiB user-install limit.");
            }
            return false;
        }
        if (!entry.isFile() || !QFile::copy(entry.absoluteFilePath(), target)) {
            if (reason != nullptr) {
                *reason = QStringLiteral("Northstar could not copy the complete bundle.");
            }
            return false;
        }
        QFileDevice::Permissions permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
        if (entry.isExecutable()) {
            permissions |= QFileDevice::ExeOwner;
        }
        if (!QFile::setPermissions(target, permissions)) {
            return false;
        }
    }
    return true;
}

bool ApplicationBundleInstaller::installBundle(const QString &sourcePath)
{
    BundleApplication source;
    if (!ApplicationBundleCatalog::inspectBundle(sourcePath, &source)) {
        setStatus(QStringLiteral("That item is not a valid Northstar application bundle."), true);
        return false;
    }

    const QFileInfo sourceInfo(source.bundlePath);
    QString reason;
    if (sourceInfo.ownerId() != processOwnerId()
        || !validateTree(source.bundlePath, sourceInfo.ownerId(), &reason)) {
        setStatus(reason.isEmpty() ? QStringLiteral("The bundle is not owned by this user.") : reason, true);
        return false;
    }
    if (!ensurePrivateDirectory(m_applicationRoot)) {
        setStatus(QStringLiteral("The user application directory is not private or writable."), true);
        return false;
    }

    const QString scope = installedScope(source.bundleId);
    if (!scope.isEmpty()) {
        setStatus(scope == QStringLiteral("system")
                      ? QStringLiteral("A package-owned application with identifier %1 is already installed.")
                            .arg(source.bundleId)
                      : QStringLiteral("An application with identifier %1 is already installed for this user.")
                            .arg(source.bundleId),
                  true);
        return false;
    }

    const QString destination = QDir(m_applicationRoot).filePath(source.bundleId + QStringLiteral(".app"));

    const QString staging = QDir(m_applicationRoot).filePath(
        QStringLiteral(".installing-") + QUuid::createUuid().toString(QUuid::WithoutBraces)
        + QStringLiteral(".app"));
    qsizetype copiedEntries = 0;
    qint64 copiedBytes = 0;
    if (!copyTree(source.bundlePath,
                  staging,
                  sourceInfo.ownerId(),
                  &copiedEntries,
                  &copiedBytes,
                  &reason)) {
        QDir(staging).removeRecursively();
        setStatus(reason, true);
        return false;
    }

    BundleApplication staged;
    if (!ApplicationBundleCatalog::inspectBundle(staging, &staged)
        || staged.bundleId != source.bundleId
        || !QDir(m_applicationRoot).rename(QFileInfo(staging).fileName(), QFileInfo(destination).fileName())) {
        QDir(staging).removeRecursively();
        setStatus(QStringLiteral("The staged application failed final validation."), true);
        return false;
    }

    setStatus(QStringLiteral("Installed %1 for this user.").arg(source.name), false);
    emit applicationsChanged();
    return true;
}

QString ApplicationBundleInstaller::installedScope(const QString &bundleIdentifier) const
{
    ApplicationBundleCatalog userCatalog({m_applicationRoot});
    for (const BundleApplication &entry : userCatalog.entries()) {
        if (entry.bundleId == bundleIdentifier) {
            return QStringLiteral("user");
        }
    }

    for (const QString &root : m_systemRoots) {
        ApplicationBundleCatalog systemCatalog({root});
        for (const BundleApplication &entry : systemCatalog.entries()) {
            if (entry.bundleId == bundleIdentifier) {
                return QStringLiteral("system");
            }
        }
    }
    return {};
}

bool ApplicationBundleInstaller::removeBundle(const QString &bundleIdentifier)
{
    ApplicationBundleCatalog catalog({m_applicationRoot});
    BundleApplication installed;
    bool found = false;
    for (const BundleApplication &entry : catalog.entries()) {
        if (entry.bundleId == bundleIdentifier) {
            installed = entry;
            found = true;
            break;
        }
    }
    if (!found || QFileInfo(installed.bundlePath).absolutePath() != cleanAbsolutePath(m_applicationRoot)) {
        setStatus(QStringLiteral("That user-installed application could not be found."), true);
        return false;
    }

    const QString trashFiles = QDir(m_trashRoot).filePath(QStringLiteral("files"));
    const QString trashInfo = QDir(m_trashRoot).filePath(QStringLiteral("info"));
    if (!ensurePrivateDirectory(m_trashRoot) || !ensurePrivateDirectory(trashFiles)
        || !ensurePrivateDirectory(trashInfo)) {
        setStatus(QStringLiteral("The user Trash directory is not private or writable."), true);
        return false;
    }

    QString trashName = QFileInfo(installed.bundlePath).fileName();
    if (QFileInfo::exists(QDir(trashFiles).filePath(trashName))
        || QFileInfo::exists(QDir(trashInfo).filePath(trashName + QStringLiteral(".trashinfo")))) {
        trashName = installed.bundleId + QLatin1Char('-')
            + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".app");
    }

    const QString infoPath = QDir(trashInfo).filePath(trashName + QStringLiteral(".trashinfo"));
    QSaveFile infoFile(infoPath);
    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Northstar could not record the Trash operation."), true);
        return false;
    }
    const QByteArray encodedPath = QUrl::toPercentEncoding(installed.bundlePath, QByteArray("/"));
    const QByteArray record = "[Trash Info]\nPath=" + encodedPath + "\nDeletionDate="
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss")).toUtf8() + '\n';
    if (infoFile.write(record) != record.size() || !infoFile.commit()
        || !QFile::setPermissions(infoPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        QFile::remove(infoPath);
        setStatus(QStringLiteral("Northstar could not commit the Trash record."), true);
        return false;
    }

    if (!QDir().rename(installed.bundlePath, QDir(trashFiles).filePath(trashName))) {
        QFile::remove(infoPath);
        setStatus(QStringLiteral("Northstar could not move the application to Trash."), true);
        return false;
    }

    setStatus(QStringLiteral("Moved %1 to Trash.").arg(installed.name), false);
    emit applicationsChanged();
    return true;
}

void ApplicationBundleInstaller::setStatus(const QString &message, bool isError)
{
    m_statusMessage = message;
    m_error = isError;
    emit statusChanged();
}
