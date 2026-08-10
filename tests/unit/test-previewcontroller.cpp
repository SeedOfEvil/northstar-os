#include "previewcontroller.h"

#include <QFile>
#include <QColor>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class PreviewControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void previewsBoundedUtf8Text();
    void previewsScaledRasterImage();
    void previewsFolderNames();
    void fallsBackToMetadata();
    void blocksPathsOutsideHome();
};

void PreviewControllerTest::previewsBoundedUtf8Text()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString path = home.filePath(QStringLiteral("notes.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("Northstar Quick Look\nUTF-8: \xE2\x98\x85\n") > 0);
    file.close();

    PreviewController controller(nullptr, home.path());
    QSignalSpy changedSpy(&controller, &PreviewController::previewChanged);
    QVERIFY(controller.previewPath(path));
    QCOMPARE(controller.status(), QStringLiteral("ready"));
    QCOMPARE(controller.kind(), QStringLiteral("text"));
    QVERIFY(controller.textContent().contains(QStringLiteral("Northstar Quick Look")));
    QVERIFY(!controller.truncated());
    QCOMPARE(changedSpy.count(), 1);
}

void PreviewControllerTest::previewsScaledRasterImage()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString path = home.filePath(QStringLiteral("wallpaper.png"));
    QImage image(1200, 800, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(QStringLiteral("#4f9fea")));
    QVERIFY(image.save(path));

    PreviewController controller(nullptr, home.path());
    QVERIFY(controller.previewPath(path));
    QCOMPARE(controller.kind(), QStringLiteral("image"));
    QVERIFY(controller.imageDataUrl().startsWith(QStringLiteral("data:image/png;base64,")));
    QVERIFY(controller.message().contains(QStringLiteral("1200")));
}

void PreviewControllerTest::previewsFolderNames()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QDir directory(home.path());
    QVERIFY(directory.mkdir(QStringLiteral("Documents")));
    QFile file(directory.filePath(QStringLiteral("Documents/hello.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hello");
    file.close();

    PreviewController controller(nullptr, home.path());
    QVERIFY(controller.previewPath(directory.filePath(QStringLiteral("Documents"))));
    QCOMPARE(controller.kind(), QStringLiteral("folder"));
    QVERIFY(controller.details().contains(QStringLiteral("hello.txt")));
    QVERIFY(controller.subtitle().contains(QStringLiteral("1 item")));
}

void PreviewControllerTest::fallsBackToMetadata()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString path = home.filePath(QStringLiteral("archive.bin"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray::fromHex("00010203ff"));
    file.close();

    PreviewController controller(nullptr, home.path());
    QVERIFY(controller.previewPath(path));
    QCOMPARE(controller.kind(), QStringLiteral("metadata"));
    QVERIFY(!controller.message().isEmpty());
    QVERIFY(controller.textContent().isEmpty());
    QVERIFY(controller.imageDataUrl().isEmpty());
}

void PreviewControllerTest::blocksPathsOutsideHome()
{
    QTemporaryDir home;
    QTemporaryDir outside;
    QVERIFY(home.isValid());
    QVERIFY(outside.isValid());
    const QString path = outside.filePath(QStringLiteral("private.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("outside");
    file.close();

    PreviewController controller(nullptr, home.path());
    QVERIFY(!controller.previewPath(path));
    QCOMPARE(controller.status(), QStringLiteral("error"));
    QCOMPARE(controller.kind(), QStringLiteral("error"));
    QVERIFY(controller.textContent().isEmpty());
}

QTEST_MAIN(PreviewControllerTest)

#include "test-previewcontroller.moc"
