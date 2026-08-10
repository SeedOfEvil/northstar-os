#include "previewcontroller.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMimeDatabase>
#include <QStorageInfo>
#include <QStringConverter>

namespace {

constexpr qint64 MaximumTextFileBytes = 8 * 1024 * 1024;
constexpr qint64 MaximumTextPreviewBytes = 128 * 1024;
constexpr qint64 MaximumImageFileBytes = 32 * 1024 * 1024;
constexpr qint64 MaximumImagePixels = 40 * 1000 * 1000;
constexpr int MaximumImageWidth = 960;
constexpr int MaximumImageHeight = 640;
constexpr int MaximumFolderEntries = 500;
constexpr int MaximumFolderDetails = 12;

} // namespace

PreviewController::PreviewController(QObject *parent, QString homePath)
    : QObject(parent)
    , m_homePath(canonicalOrNormalizedPath(homePath.isEmpty() ? QDir::homePath() : homePath))
{
}

QString PreviewController::status() const { return m_status; }
QString PreviewController::kind() const { return m_kind; }
QString PreviewController::path() const { return m_path; }
QString PreviewController::title() const { return m_title; }
QString PreviewController::subtitle() const { return m_subtitle; }
QString PreviewController::mimeType() const { return m_mimeType; }
QString PreviewController::textContent() const { return m_textContent; }
QString PreviewController::imageDataUrl() const { return m_imageDataUrl; }
QStringList PreviewController::details() const { return m_details; }
QString PreviewController::message() const { return m_message; }
bool PreviewController::truncated() const { return m_truncated; }

bool PreviewController::previewPath(const QString &path, const QString &navigationRoot)
{
    resetValues();

    const QFileInfo requestedInfo(path);
    const QString resolvedPath = requestedInfo.canonicalFilePath();
    if (resolvedPath.isEmpty() || !requestedInfo.exists()) {
        setError(QStringLiteral("Preview unavailable"),
                 QStringLiteral("This item no longer exists or cannot be resolved."));
        return false;
    }
    if (!isAllowedPath(resolvedPath, navigationRoot)) {
        setError(QStringLiteral("Preview blocked"),
                 QStringLiteral("Quick Look only previews Home or the explicitly browsed mounted volume."));
        return false;
    }

    const QFileInfo info(resolvedPath);
    if (info.isSymLink()) {
        setError(QStringLiteral("Preview unavailable"),
                 QStringLiteral("Symbolic links are not previewed."));
        return false;
    }

    m_path = resolvedPath;
    m_title = info.fileName().isEmpty() ? resolvedPath : info.fileName();
    if (info.isDir()) {
        previewDirectory(resolvedPath);
    } else if (info.isFile()) {
        previewFile(resolvedPath);
    } else {
        m_subtitle = QStringLiteral("Special filesystem item");
        setMetadataOnly(QStringLiteral("This item type has no visual preview."));
    }
    emit previewChanged();
    return m_status == QStringLiteral("ready");
}

void PreviewController::clear()
{
    resetValues();
    emit previewChanged();
}

QString PreviewController::canonicalOrNormalizedPath(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return QDir::cleanPath(canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath);
}

bool PreviewController::pathMatchesRoot(const QString &path, const QString &root)
{
    return !root.isEmpty() && (path == root || path.startsWith(root + QLatin1Char('/')));
}

QString PreviewController::formattedSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    }
    if (bytes < 1024LL * 1024LL * 1024LL) {
        return QStringLiteral("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
    }
    return QStringLiteral("%1 GB").arg(QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1));
}

QString PreviewController::formattedTimestamp(const QString &path)
{
    return QFileInfo(path).lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

bool PreviewController::isAllowedPath(const QString &path, const QString &navigationRoot) const
{
    if (pathMatchesRoot(path, m_homePath)) {
        return true;
    }
    const QString root = canonicalOrNormalizedPath(navigationRoot);
    return pathMatchesRoot(path, root) && isExplicitMountedRoot(root);
}

bool PreviewController::isExplicitMountedRoot(const QString &root) const
{
    if (root.isEmpty() || root == QStringLiteral("/") || root == m_homePath) {
        return false;
    }
    const QStorageInfo storage(root);
    if (!storage.isValid() || !storage.isReady()) {
        return false;
    }
    return canonicalOrNormalizedPath(storage.rootPath()) == root;
}

void PreviewController::previewDirectory(const QString &path)
{
    m_kind = QStringLiteral("folder");
    m_status = QStringLiteral("ready");
    m_mimeType = QStringLiteral("inode/directory");

    int entryCount = 0;
    QDirIterator iterator(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Readable);
    while (iterator.hasNext() && entryCount <= MaximumFolderEntries) {
        iterator.next();
        const QFileInfo info = iterator.fileInfo();
        if (info.isSymLink()) {
            continue;
        }
        if (entryCount < MaximumFolderDetails) {
            m_details.append(info.fileName() + (info.isDir() ? QStringLiteral("/") : QString()));
        }
        ++entryCount;
    }

    m_truncated = entryCount > MaximumFolderEntries;
    const QString countText = m_truncated
        ? QStringLiteral("More than %1 items").arg(MaximumFolderEntries)
        : QStringLiteral("%1 item%2").arg(entryCount).arg(entryCount == 1 ? QString() : QStringLiteral("s"));
    m_subtitle = QStringLiteral("Folder · %1 · Modified %2").arg(countText, formattedTimestamp(path));
    if (m_details.isEmpty()) {
        m_message = QStringLiteral("This folder is empty.");
    } else if (m_truncated) {
        m_message = QStringLiteral("Showing the first %1 names from a bounded folder scan.")
                        .arg(MaximumFolderDetails);
    }
}

void PreviewController::previewFile(const QString &path)
{
    const QFileInfo info(path);
    const qint64 fileSize = info.size();
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchContent);
    m_mimeType = mime.name();
    m_subtitle = QStringLiteral("%1 · %2 · Modified %3")
                     .arg(m_mimeType.isEmpty() ? QStringLiteral("File") : m_mimeType,
                          formattedSize(fileSize), formattedTimestamp(path));

    if (m_mimeType.startsWith(QStringLiteral("image/"))) {
        if (fileSize > MaximumImageFileBytes) {
            setMetadataOnly(QStringLiteral("This image is too large for bounded Quick Look."));
            return;
        }
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize sourceSize = reader.size();
        if (!sourceSize.isValid()
            || static_cast<qint64>(sourceSize.width()) * sourceSize.height() > MaximumImagePixels) {
            setMetadataOnly(QStringLiteral("This image exceeds the safe preview dimensions."));
            return;
        }
        QSize previewSize = sourceSize;
        previewSize.scale(MaximumImageWidth, MaximumImageHeight, Qt::KeepAspectRatio);
        if (previewSize != sourceSize) {
            reader.setScaledSize(previewSize);
        }
        const QImage image = reader.read();
        if (image.isNull()) {
            setMetadataOnly(QStringLiteral("Northstar could not decode this raster image."));
            return;
        }
        QByteArray encoded;
        QBuffer buffer(&encoded);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) {
            setMetadataOnly(QStringLiteral("Northstar could not prepare this image preview."));
            return;
        }
        m_kind = QStringLiteral("image");
        m_status = QStringLiteral("ready");
        m_imageDataUrl = QStringLiteral("data:image/png;base64,")
                         + QString::fromLatin1(encoded.toBase64());
        m_message = QStringLiteral("%1 × %2 pixels").arg(sourceSize.width()).arg(sourceSize.height());
        return;
    }

    if (m_mimeType.startsWith(QStringLiteral("text/"))) {
        if (fileSize > MaximumTextFileBytes) {
            setMetadataOnly(QStringLiteral("This text file is too large for bounded Quick Look."));
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            setError(QStringLiteral("Preview unavailable"), file.errorString());
            return;
        }
        const QByteArray bytes = file.read(MaximumTextPreviewBytes + 1);
        if (bytes.contains('\0')) {
            setMetadataOnly(QStringLiteral("This file does not contain plain UTF-8 text."));
            return;
        }
        QStringDecoder decoder(QStringDecoder::Utf8);
        QString decoded = decoder.decode(bytes.left(MaximumTextPreviewBytes));
        if (decoder.hasError()) {
            setMetadataOnly(QStringLiteral("This file is not valid UTF-8 text."));
            return;
        }
        m_kind = QStringLiteral("text");
        m_status = QStringLiteral("ready");
        m_textContent = decoded;
        m_truncated = fileSize > MaximumTextPreviewBytes;
        if (m_truncated) {
            m_message = QStringLiteral("Showing the first 128 KB. The file was not fully loaded.");
        }
        return;
    }

    setMetadataOnly(QStringLiteral("No Quick Look renderer is available for this file type."));
}

void PreviewController::setMetadataOnly(const QString &reason)
{
    m_kind = QStringLiteral("metadata");
    m_status = QStringLiteral("ready");
    m_message = reason;
}

void PreviewController::setError(const QString &title, const QString &message)
{
    resetValues();
    m_status = QStringLiteral("error");
    m_kind = QStringLiteral("error");
    m_title = title;
    m_message = message;
    emit previewChanged();
}

void PreviewController::resetValues()
{
    m_status = QStringLiteral("empty");
    m_kind = QStringLiteral("metadata");
    m_path.clear();
    m_title.clear();
    m_subtitle.clear();
    m_mimeType.clear();
    m_textContent.clear();
    m_imageDataUrl.clear();
    m_details.clear();
    m_message.clear();
    m_truncated = false;
}
