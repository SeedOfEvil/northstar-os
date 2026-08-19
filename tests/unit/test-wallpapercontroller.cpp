#include "wallpapercontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVariantMap>

// Every case pins the controller to its own settings file inside a temporary
// directory. A controller left on its default path would read and rewrite the
// real desktop's wallpaper preference, which makes the result depend on the
// machine running the suite rather than on the code.
class WallpaperControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void startsOnTheBuiltInBackground();
    void acceptsAPictureAndRestoresItNextSession();
    void acceptsAFileUrlAsWellAsAPath();
    void refusesAFileThatIsNotAPictureDespiteItsName();
    void refusesAMissingFile();
    void refusesADirectory();
    void refusesAPictureLargerThanTheSideLimit();
    void fallsBackWhenTheStoredPictureIsGone();
    void revalidateDropsAPictureDeletedDuringTheSession();
    void revalidateLeavesAPictureThatIsStillThere();
    void keepsTheCurrentFitWhenAskedForAnUnknownOne();
    void acceptsEveryFitItOffers();
    void listsPicturesAsSelectableAndOtherFilesNot();
    void browsesIntoAndBackOutOfFolders();
    void picturesShortcutLandsSomewhereReadable();

private:
    QString settingsPath() const;
    QString writePicture(const QString &name, int width = 8, int height = 8) const;
    QString writeFile(const QString &name, const QByteArray &contents) const;

    QTemporaryDir *m_directory = nullptr;
};

void WallpaperControllerTest::init()
{
    m_directory = new QTemporaryDir;
    QVERIFY(m_directory->isValid());
}

void WallpaperControllerTest::cleanup()
{
    delete m_directory;
    m_directory = nullptr;
}

QString WallpaperControllerTest::settingsPath() const
{
    return m_directory->filePath(QStringLiteral("wallpaper.ini"));
}

QString WallpaperControllerTest::writePicture(const QString &name, int width, int height) const
{
    const QString path = m_directory->filePath(name);
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::blue);
    if (!image.save(path, "PNG")) {
        return QString();
    }
    return path;
}

QString WallpaperControllerTest::writeFile(const QString &name, const QByteArray &contents) const
{
    const QString path = m_directory->filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write(contents);
    file.close();
    return path;
}

void WallpaperControllerTest::startsOnTheBuiltInBackground()
{
    WallpaperController wallpaper(nullptr, settingsPath());

    QVERIFY(!wallpaper.hasImage());
    QVERIFY(wallpaper.imagePath().isEmpty());
    QVERIFY(wallpaper.imageSource().isEmpty());
    QCOMPARE(wallpaper.fitMode(), WallpaperController::defaultFitMode());
    QVERIFY(!wallpaper.statusIsError());
}

void WallpaperControllerTest::acceptsAPictureAndRestoresItNextSession()
{
    const QString picture = writePicture(QStringLiteral("desert.png"));
    QVERIFY(!picture.isEmpty());

    {
        WallpaperController wallpaper(nullptr, settingsPath());
        QSignalSpy changed(&wallpaper, &WallpaperController::wallpaperChanged);

        QVERIFY(wallpaper.setImagePath(picture));
        QCOMPARE(changed.count(), 1);
        QVERIFY(wallpaper.hasImage());
        QCOMPARE(wallpaper.imagePath(), QFileInfo(picture).canonicalFilePath());
        QVERIFY(!wallpaper.statusIsError());
        QVERIFY(wallpaper.setFitMode(QStringLiteral("centre")));
    }

    // A second controller on the same settings file is what the next login
    // actually looks like.
    WallpaperController restored(nullptr, settingsPath());
    QVERIFY(restored.hasImage());
    QCOMPARE(restored.imagePath(), QFileInfo(picture).canonicalFilePath());
    QCOMPARE(restored.fitMode(), QStringLiteral("centre"));
    QVERIFY(!restored.statusIsError());
}

void WallpaperControllerTest::acceptsAFileUrlAsWellAsAPath()
{
    const QString picture = writePicture(QStringLiteral("lake.png"));
    QVERIFY(!picture.isEmpty());

    WallpaperController wallpaper(nullptr, settingsPath());
    QVERIFY(wallpaper.setImagePath(QUrl::fromLocalFile(picture).toString()));
    QCOMPARE(wallpaper.imagePath(), QFileInfo(picture).canonicalFilePath());
}

void WallpaperControllerTest::refusesAFileThatIsNotAPictureDespiteItsName()
{
    // The name says PNG and the contents say otherwise. Trusting the suffix
    // is exactly how a desktop ends up trying to draw a text file.
    const QString impostor = writeFile(QStringLiteral("notes.png"),
                                       QByteArrayLiteral("this is not a picture at all"));
    QVERIFY(!impostor.isEmpty());

    WallpaperController wallpaper(nullptr, settingsPath());
    QSignalSpy changed(&wallpaper, &WallpaperController::wallpaperChanged);

    QVERIFY(!wallpaper.setImagePath(impostor));
    QCOMPARE(changed.count(), 0);
    QVERIFY(!wallpaper.hasImage());
    QVERIFY(wallpaper.statusIsError());
    QVERIFY(wallpaper.status().contains(QStringLiteral("notes.png")));
}

void WallpaperControllerTest::refusesAMissingFile()
{
    WallpaperController wallpaper(nullptr, settingsPath());

    QVERIFY(!wallpaper.setImagePath(m_directory->filePath(QStringLiteral("absent.png"))));
    QVERIFY(!wallpaper.hasImage());
    QVERIFY(wallpaper.statusIsError());
}

void WallpaperControllerTest::refusesADirectory()
{
    const QString folder = m_directory->filePath(QStringLiteral("pictures"));
    QVERIFY(QDir().mkpath(folder));

    WallpaperController wallpaper(nullptr, settingsPath());

    QVERIFY(!wallpaper.setImagePath(folder));
    QVERIFY(!wallpaper.hasImage());
    QVERIFY(wallpaper.statusIsError());
}

void WallpaperControllerTest::refusesAPictureLargerThanTheSideLimit()
{
    // Wide and one pixel tall: a small file that still exceeds the per-side
    // limit, so the check can be exercised without allocating a huge image.
    const int oversized = WallpaperController::maximumPixelDimension() + 1000;
    const QString wide = writePicture(QStringLiteral("panorama.png"), oversized, 1);
    QVERIFY(!wide.isEmpty());

    WallpaperController wallpaper(nullptr, settingsPath());

    QVERIFY(!wallpaper.setImagePath(wide));
    QVERIFY(!wallpaper.hasImage());
    QVERIFY(wallpaper.statusIsError());
    QVERIFY(wallpaper.status().contains(QStringLiteral("pixels on a side")));
}

void WallpaperControllerTest::fallsBackWhenTheStoredPictureIsGone()
{
    const QString picture = writePicture(QStringLiteral("temporary.png"));
    QVERIFY(!picture.isEmpty());

    {
        WallpaperController wallpaper(nullptr, settingsPath());
        QVERIFY(wallpaper.setImagePath(picture));
    }

    QVERIFY(QFile::remove(picture));

    // The next login must start on the built-in background and say why,
    // rather than presenting an empty desktop with no explanation.
    WallpaperController restored(nullptr, settingsPath());
    QVERIFY(!restored.hasImage());
    QVERIFY(restored.statusIsError());
    QVERIFY(!restored.status().isEmpty());
}

void WallpaperControllerTest::revalidateDropsAPictureDeletedDuringTheSession()
{
    const QString picture = writePicture(QStringLiteral("session.png"));
    QVERIFY(!picture.isEmpty());

    WallpaperController wallpaper(nullptr, settingsPath());
    QVERIFY(wallpaper.setImagePath(picture));
    QVERIFY(wallpaper.hasImage());

    QSignalSpy changed(&wallpaper, &WallpaperController::wallpaperChanged);
    QVERIFY(QFile::remove(picture));
    wallpaper.revalidate();

    QCOMPARE(changed.count(), 1);
    QVERIFY(!wallpaper.hasImage());
    QVERIFY(wallpaper.statusIsError());
}

void WallpaperControllerTest::revalidateLeavesAPictureThatIsStillThere()
{
    const QString picture = writePicture(QStringLiteral("stable.png"));
    QVERIFY(!picture.isEmpty());

    WallpaperController wallpaper(nullptr, settingsPath());
    QVERIFY(wallpaper.setImagePath(picture));

    QSignalSpy changed(&wallpaper, &WallpaperController::wallpaperChanged);
    wallpaper.revalidate();

    QCOMPARE(changed.count(), 0);
    QVERIFY(wallpaper.hasImage());
}

void WallpaperControllerTest::keepsTheCurrentFitWhenAskedForAnUnknownOne()
{
    WallpaperController wallpaper(nullptr, settingsPath());
    QVERIFY(wallpaper.setFitMode(QStringLiteral("tile")));

    QVERIFY(!wallpaper.setFitMode(QStringLiteral("diagonal")));
    QCOMPARE(wallpaper.fitMode(), QStringLiteral("tile"));
    QVERIFY(wallpaper.statusIsError());
}

void WallpaperControllerTest::acceptsEveryFitItOffers()
{
    WallpaperController wallpaper(nullptr, settingsPath());

    // Whatever the surface offers has to be something the controller takes.
    const QStringList offered = wallpaper.availableFitModes();
    QVERIFY(!offered.isEmpty());
    for (const QString &mode : offered) {
        QVERIFY2(wallpaper.setFitMode(mode), qPrintable(mode));
        QCOMPARE(wallpaper.fitMode(), mode);
        QVERIFY2(!wallpaper.labelForFitMode(mode).isEmpty(), qPrintable(mode));
    }
}

void WallpaperControllerTest::listsPicturesAsSelectableAndOtherFilesNot()
{
    QVERIFY(!writePicture(QStringLiteral("photo.png")).isEmpty());
    QVERIFY(!writeFile(QStringLiteral("readme.txt"), QByteArrayLiteral("text")).isEmpty());
    QVERIFY(QDir().mkpath(m_directory->filePath(QStringLiteral("album"))));

    WallpaperController wallpaper(nullptr, settingsPath());
    QVERIFY(wallpaper.browseTo(m_directory->path()));

    bool sawPicture = false;
    bool sawText = false;
    bool sawFolder = false;
    for (const QVariant &entry : wallpaper.browseEntries()) {
        const QVariantMap map = entry.toMap();
        const QString name = map.value(QStringLiteral("name")).toString();
        if (name == QStringLiteral("photo.png")) {
            sawPicture = true;
            QVERIFY(map.value(QStringLiteral("selectable")).toBool());
        } else if (name == QStringLiteral("readme.txt")) {
            sawText = true;
            QVERIFY(!map.value(QStringLiteral("selectable")).toBool());
            QCOMPARE(map.value(QStringLiteral("reason")).toString(),
                     QStringLiteral("Not a picture"));
        } else if (name == QStringLiteral("album")) {
            sawFolder = true;
            QVERIFY(map.value(QStringLiteral("isDirectory")).toBool());
            // A folder stays enterable even though it is not a picture.
            QVERIFY(map.value(QStringLiteral("selectable")).toBool());
        }
    }
    QVERIFY(sawPicture);
    QVERIFY(sawText);
    QVERIFY(sawFolder);
}

void WallpaperControllerTest::browsesIntoAndBackOutOfFolders()
{
    const QString album = m_directory->filePath(QStringLiteral("album"));
    QVERIFY(QDir().mkpath(album));

    WallpaperController wallpaper(nullptr, settingsPath());
    QVERIFY(wallpaper.browseTo(m_directory->path()));
    QVERIFY(wallpaper.browseTo(album));
    QCOMPARE(QDir(wallpaper.browsePath()).dirName(), QStringLiteral("album"));

    QVERIFY(wallpaper.browseCanNavigateUp());
    QVERIFY(wallpaper.browseUp());
    QCOMPARE(QDir(wallpaper.browsePath()).canonicalPath(),
             QDir(m_directory->path()).canonicalPath());

    QVERIFY(!wallpaper.browseTo(m_directory->filePath(QStringLiteral("no-such-folder"))));
}

void WallpaperControllerTest::picturesShortcutLandsSomewhereReadable()
{
    // Whether this machine has a Pictures folder is not something the test
    // can assert, so it asserts the guarantee that holds either way: the
    // shortcut always leaves the browser in a readable directory.
    WallpaperController wallpaper(nullptr, settingsPath());

    QVERIFY(wallpaper.browseToPictures());
    QVERIFY(!wallpaper.browsePath().isEmpty());
    QVERIFY(QFileInfo(wallpaper.browsePath()).isDir());
    QVERIFY(!wallpaper.browseDisplayPath().isEmpty());
}

QTEST_MAIN(WallpaperControllerTest)
#include "test-wallpapercontroller.moc"
