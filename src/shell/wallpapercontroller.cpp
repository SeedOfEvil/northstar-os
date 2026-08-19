#include "wallpapercontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantMap>

namespace {

const QLatin1String FillMode("fill");
const QLatin1String FitMode("fit");
const QLatin1String StretchMode("stretch");
const QLatin1String CentreMode("centre");
const QLatin1String TileMode("tile");

constexpr qint64 MaximumFileBytes = 64LL * 1024 * 1024;
constexpr int MaximumPixelDimension = 16384;
constexpr qint64 MaximumPixelCount = 40000000;
constexpr int MaximumBrowseEntries = 2000;

// The listing marks candidates by suffix rather than by opening every file in
// the folder, which would mean one file open per entry just to draw a list.
// The real check happens when a picture is actually chosen, which is where
// being wrong would matter.
QStringList pictureSuffixes()
{
    QStringList suffixes;
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    suffixes.reserve(formats.size());
    for (const QByteArray &format : formats) {
        suffixes.append(QString::fromLatin1(format).toLower());
    }
    return suffixes;
}

QString sizeSummary(qint64 bytes)
{
    if (bytes >= 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
    }
    if (bytes >= 1024) {
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    }
    return QStringLiteral("%1 bytes").arg(bytes);
}

QString defaultSettingsPath()
{
    QString configDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDirectory.isEmpty()) {
        configDirectory = QDir::home().filePath(QStringLiteral(".config/northstar"));
    }
    return QDir(configDirectory).filePath(QStringLiteral("wallpaper.ini"));
}

// The dialog hands back a file:// URL and the settings field hands back a
// path. Both mean the same thing, so both are accepted here rather than at
// every call site.
QString localPathFor(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    if (trimmed.startsWith(QLatin1String("file:"))) {
        return QUrl(trimmed).toLocalFile();
    }
    return QDir::cleanPath(QDir::fromNativeSeparators(trimmed));
}

} // namespace

WallpaperController::WallpaperController(QObject *parent, QString settingsPath)
    : QObject(parent)
    , m_settingsPath(settingsPath.trimmed().isEmpty()
            ? defaultSettingsPath()
            : QDir::cleanPath(QDir::fromNativeSeparators(settingsPath)))
    , m_fitMode(defaultFitMode())
{
    loadPreferences();
}

QStringList WallpaperController::fitModes()
{
    return QStringList{FillMode, FitMode, StretchMode, CentreMode, TileMode};
}

QString WallpaperController::defaultFitMode()
{
    return FillMode;
}

bool WallpaperController::isFitMode(const QString &mode)
{
    return fitModes().contains(mode);
}

QString WallpaperController::fitModeLabel(const QString &mode)
{
    if (mode == FillMode) {
        return QStringLiteral("Fill screen");
    }
    if (mode == FitMode) {
        return QStringLiteral("Fit to screen");
    }
    if (mode == StretchMode) {
        return QStringLiteral("Stretch");
    }
    if (mode == CentreMode) {
        return QStringLiteral("Centre");
    }
    if (mode == TileMode) {
        return QStringLiteral("Tile");
    }
    return QString();
}

qint64 WallpaperController::maximumFileBytes()
{
    return MaximumFileBytes;
}

int WallpaperController::maximumPixelDimension()
{
    return MaximumPixelDimension;
}

qint64 WallpaperController::maximumPixelCount()
{
    return MaximumPixelCount;
}

QString WallpaperController::imagePath() const
{
    return m_imagePath;
}

QUrl WallpaperController::imageSource() const
{
    return m_imagePath.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_imagePath);
}

bool WallpaperController::hasImage() const
{
    return !m_imagePath.isEmpty();
}

QString WallpaperController::fitMode() const
{
    return m_fitMode;
}

QString WallpaperController::status() const
{
    return m_status;
}

bool WallpaperController::statusIsError() const
{
    return m_statusIsError;
}

QStringList WallpaperController::availableFitModes() const
{
    return fitModes();
}

QString WallpaperController::labelForFitMode(const QString &mode) const
{
    return fitModeLabel(mode);
}

QString WallpaperController::browsePath() const
{
    return m_browsePath;
}

QString WallpaperController::browseDisplayPath() const
{
    if (m_browsePath.isEmpty()) {
        return QString();
    }
    const QString home = QDir::homePath();
    if (m_browsePath == home) {
        return QStringLiteral("Home");
    }
    if (m_browsePath.startsWith(home + QLatin1Char('/'))) {
        return QStringLiteral("Home/") + m_browsePath.mid(home.size() + 1);
    }
    return m_browsePath;
}

QVariantList WallpaperController::browseEntries() const
{
    return m_browseEntries;
}

bool WallpaperController::browseCanNavigateUp() const
{
    QDir directory(m_browsePath);
    return !m_browsePath.isEmpty() && directory.cdUp();
}

bool WallpaperController::browseTruncated() const
{
    return m_browseTruncated;
}

bool WallpaperController::browseTo(const QString &path)
{
    const QString resolved = localPathFor(path);
    const QFileInfo info(resolved);
    if (resolved.isEmpty() || !info.exists() || !info.isDir()) {
        announce(QStringLiteral("%1 is not a folder that can be browsed.")
                     .arg(path.trimmed().isEmpty() ? QStringLiteral("That location")
                                                   : path.trimmed()),
                 true);
        return false;
    }

    QDir directory(resolved);
    if (!info.isReadable() || !directory.isReadable()) {
        announce(QStringLiteral("Unable to list %1. Check that you have permission to open it.")
                     .arg(info.fileName().isEmpty() ? resolved : info.fileName()),
                 true);
        return false;
    }

    m_browsePath = directory.absolutePath();
    refreshBrowse();
    return true;
}

bool WallpaperController::browseUp()
{
    QDir directory(m_browsePath);
    if (!directory.cdUp()) {
        return false;
    }
    return browseTo(directory.absolutePath());
}

bool WallpaperController::browseHome()
{
    return browseTo(QDir::homePath());
}

bool WallpaperController::browseToPictures()
{
    // Start where pictures usually are, but only if that folder exists on
    // this system; otherwise Home is the honest starting point.
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (!pictures.isEmpty() && QFileInfo(pictures).isDir()) {
        return browseTo(pictures);
    }
    return browseHome();
}

void WallpaperController::refreshBrowse()
{
    m_browseEntries.clear();
    m_browseTruncated = false;
    if (m_browsePath.isEmpty()) {
        emit browseChanged();
        return;
    }

    const QStringList suffixes = pictureSuffixes();
    const QDir directory(m_browsePath);
    const QFileInfoList infos =
        directory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &info : infos) {
        if (m_browseEntries.size() >= MaximumBrowseEntries) {
            m_browseTruncated = true;
            break;
        }

        const bool isDirectory = info.isDir();
        const bool looksLikePicture = suffixes.contains(info.suffix().toLower());
        const bool tooLarge = !isDirectory && info.size() > MaximumFileBytes;
        const bool selectable = isDirectory
            ? info.isReadable()
            : info.isFile() && info.isReadable() && looksLikePicture && !tooLarge;

        QString reason;
        if (!isDirectory && !looksLikePicture) {
            reason = QStringLiteral("Not a picture");
        } else if (tooLarge) {
            reason = QStringLiteral("Larger than %1").arg(sizeSummary(MaximumFileBytes));
        } else if (!selectable) {
            reason = QStringLiteral("Not readable");
        }

        m_browseEntries.append(QVariantMap{
            {QStringLiteral("name"), info.fileName()},
            {QStringLiteral("path"), info.absoluteFilePath()},
            {QStringLiteral("isDirectory"), isDirectory},
            {QStringLiteral("selectable"), selectable},
            {QStringLiteral("size"), isDirectory ? QString() : sizeSummary(info.size())},
            {QStringLiteral("reason"), reason},
        });
    }

    emit browseChanged();
}

bool WallpaperController::setImagePath(const QString &path)
{
    const QString requested = localPathFor(path);
    if (requested.isEmpty()) {
        return clearImage();
    }

    QString reason;
    const QString resolved = validatedImagePath(requested, &reason);
    if (resolved.isEmpty()) {
        announce(reason, true);
        return false;
    }

    if (resolved != m_imagePath) {
        m_imagePath = resolved;
        savePreferences();
        emit wallpaperChanged();
    }
    announce(QStringLiteral("Desktop background set to %1.").arg(QFileInfo(resolved).fileName()),
             false);
    return true;
}

bool WallpaperController::setFitMode(const QString &mode)
{
    const QString requested = mode.trimmed().toLower();
    if (!isFitMode(requested)) {
        announce(QStringLiteral("%1 is not a desktop background fit.").arg(mode.trimmed()), true);
        return false;
    }

    if (requested != m_fitMode) {
        m_fitMode = requested;
        savePreferences();
        emit wallpaperChanged();
    }
    announce(
        QStringLiteral("Desktop background fit set to %1.").arg(fitModeLabel(requested).toLower()),
        false);
    return true;
}

bool WallpaperController::clearImage()
{
    if (!m_imagePath.isEmpty()) {
        m_imagePath.clear();
        savePreferences();
        emit wallpaperChanged();
    }
    announce(QStringLiteral("Using the built-in Northstar background."), false);
    return true;
}

void WallpaperController::revalidate()
{
    if (m_imagePath.isEmpty()) {
        return;
    }

    QString reason;
    if (!validatedImagePath(m_imagePath, &reason).isEmpty()) {
        return;
    }

    // The stored picture is gone or no longer readable. Fall back rather than
    // leave the desktop showing something that cannot be reloaded, and keep
    // the reason so Settings can say what happened.
    m_imagePath.clear();
    savePreferences();
    announce(reason, true);
    emit wallpaperChanged();
}

QString WallpaperController::validatedImagePath(const QString &path, QString *reason)
{
    const auto refuse = [reason](const QString &message) {
        if (reason) {
            *reason = message;
        }
        return QString();
    };

    const QFileInfo info(path);
    if (!info.exists()) {
        return refuse(QStringLiteral("That picture is no longer at %1.").arg(path));
    }
    if (!info.isFile()) {
        return refuse(QStringLiteral("%1 is not a file.").arg(path));
    }
    if (!info.isReadable()) {
        return refuse(QStringLiteral("%1 cannot be read.").arg(info.fileName()));
    }
    if (info.size() > MaximumFileBytes) {
        return refuse(QStringLiteral("%1 is larger than the %2 MB desktop background limit.")
                          .arg(info.fileName())
                          .arg(MaximumFileBytes / (1024 * 1024)));
    }

    // Read the file rather than trust its name: an extension says nothing
    // about what a file actually contains.
    QImageReader reader(info.absoluteFilePath());
    if (!reader.canRead()) {
        return refuse(
            QStringLiteral("%1 is not a picture Northstar can display.").arg(info.fileName()));
    }

    // A file small on disk can still be enormous once decoded, and the
    // decoded copy is what the shell holds in memory. Both limits exist
    // because either one alone lets a pathological picture through: a very
    // long thin image passes a megapixel test, and a modest square one can
    // still be far more than the desktop needs.
    const QSize size = reader.size();
    if (size.isValid()) {
        if (size.width() > MaximumPixelDimension || size.height() > MaximumPixelDimension) {
            return refuse(QStringLiteral("%1 is larger than %2 pixels on a side.")
                              .arg(info.fileName())
                              .arg(MaximumPixelDimension));
        }
        const qint64 pixels = qint64(size.width()) * qint64(size.height());
        if (pixels > MaximumPixelCount) {
            return refuse(QStringLiteral("%1 is larger than %2 megapixels.")
                              .arg(info.fileName())
                              .arg(MaximumPixelCount / 1000000));
        }
    }

    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

void WallpaperController::loadPreferences()
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    const QString storedFit =
        settings.value(QStringLiteral("wallpaper/fit")).toString().trimmed().toLower();
    if (isFitMode(storedFit)) {
        m_fitMode = storedFit;
    }

    const QString storedPath =
        localPathFor(settings.value(QStringLiteral("wallpaper/image")).toString());
    if (storedPath.isEmpty()) {
        return;
    }

    QString reason;
    const QString resolved = validatedImagePath(storedPath, &reason);
    if (resolved.isEmpty()) {
        // A wallpaper set in an earlier session can be gone by this one. Start
        // from the built-in background and say why, rather than presenting an
        // empty desktop with no explanation.
        m_status = reason;
        m_statusIsError = true;
        return;
    }
    m_imagePath = resolved;
}

void WallpaperController::savePreferences() const
{
    const QFileInfo settingsInfo(m_settingsPath);
    if (!QDir().mkpath(settingsInfo.absolutePath())) {
        return;
    }

    QSettings settings(m_settingsPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("wallpaper/image"), m_imagePath);
    settings.setValue(QStringLiteral("wallpaper/fit"), m_fitMode);
    settings.sync();
}

void WallpaperController::announce(const QString &message, bool error)
{
    if (m_status == message && m_statusIsError == error) {
        return;
    }
    m_status = message;
    m_statusIsError = error;
    emit statusChanged();
}
