#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

// Owns the desktop background image and how it is fitted to the screen.
//
// A wallpaper is a path into the user's own filesystem, so it can be moved,
// deleted, or replaced by something that is not an image at all between one
// login and the next. The controller therefore validates by reading the file
// rather than trusting its name, and a stored wallpaper that no longer
// resolves falls back to the built-in Northstar background with a stated
// reason instead of leaving the desktop blank.
class WallpaperController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString imagePath READ imagePath NOTIFY wallpaperChanged)
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY wallpaperChanged)
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY wallpaperChanged)
    Q_PROPERTY(QString fitMode READ fitMode NOTIFY wallpaperChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusChanged)
    Q_PROPERTY(QString browsePath READ browsePath NOTIFY browseChanged)
    Q_PROPERTY(QString browseDisplayPath READ browseDisplayPath NOTIFY browseChanged)
    Q_PROPERTY(QVariantList browseEntries READ browseEntries NOTIFY browseChanged)
    Q_PROPERTY(bool browseCanNavigateUp READ browseCanNavigateUp NOTIFY browseChanged)
    Q_PROPERTY(bool browseTruncated READ browseTruncated NOTIFY browseChanged)

public:
    explicit WallpaperController(QObject *parent = nullptr, QString settingsPath = {});

    // Fit modes, in the order Settings offers them. "fill" is the default
    // because it is the only one that covers the whole screen without
    // distorting the picture.
    static QStringList fitModes();
    static QString defaultFitMode();
    static bool isFitMode(const QString &mode);
    static QString fitModeLabel(const QString &mode);

    // The largest picture the shell will load. A wallpaper is decoded into
    // memory on every display, so an unbounded one is a way to stall the
    // session by choosing the wrong file.
    static qint64 maximumFileBytes();
    static int maximumPixelDimension();
    static qint64 maximumPixelCount();

    QString imagePath() const;
    QUrl imageSource() const;
    bool hasImage() const;
    QString fitMode() const;
    QString status() const;
    bool statusIsError() const;

    QString browsePath() const;
    QString browseDisplayPath() const;
    QVariantList browseEntries() const;
    bool browseCanNavigateUp() const;
    bool browseTruncated() const;

    Q_INVOKABLE QStringList availableFitModes() const;
    Q_INVOKABLE QString labelForFitMode(const QString &mode) const;

    // The in-shell picture browser, following the same shape the text editor
    // uses to open a document. A platform file dialog is not used because the
    // shell surfaces are layer-shell windows and the project owns its own
    // browsing surfaces.
    Q_INVOKABLE bool browseTo(const QString &path);
    Q_INVOKABLE bool browseUp();
    Q_INVOKABLE bool browseHome();
    Q_INVOKABLE bool browseToPictures();

public slots:
    // Accepts a path or a file:// URL, so the same entry point serves the
    // settings field and the picture browser.
    bool setImagePath(const QString &path);
    bool setFitMode(const QString &mode);
    bool clearImage();

    // Re-checks the stored picture against the filesystem. The desktop calls
    // this when it is refreshed, so a wallpaper deleted underneath a running
    // session is reported rather than left on screen as a stale texture.
    void revalidate();

signals:
    void wallpaperChanged();
    void statusChanged();
    void browseChanged();

private:
    void refreshBrowse();

    // Returns the canonical path when the file is a usable image, and an empty
    // string otherwise, setting *reason to why it was refused.
    static QString validatedImagePath(const QString &path, QString *reason);

    void loadPreferences();
    void savePreferences() const;
    void announce(const QString &message, bool error);

    QString m_settingsPath;
    QString m_imagePath;
    QString m_fitMode;
    QString m_status;
    bool m_statusIsError = false;
    QString m_browsePath;
    QVariantList m_browseEntries;
    bool m_browseTruncated = false;
};
