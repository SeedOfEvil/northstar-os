#include "texteditorcontroller.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TextEditorControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsAndSavesText();
    void savesNewDocument();
    void rejectsMissingFiles();
};

void TextEditorControllerTest::loadsAndSavesText()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("notes.txt"));

    QFile source(path);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("Northstar"), qint64(9));
    source.close();

    TextEditorController controller;
    QVERIFY(controller.loadFile(path));
    QCOMPARE(controller.text(), QStringLiteral("Northstar"));
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());

    controller.setText(QStringLiteral("Northstar editor"));
    QVERIFY(controller.dirty());
    QVERIFY(controller.canSave());
    QVERIFY(controller.save());

    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(saved.readAll(), QByteArray("Northstar editor"));
    QVERIFY(!controller.dirty());
}

void TextEditorControllerTest::rejectsMissingFiles()
{
    TextEditorController controller;
    QVERIFY(!controller.loadFile(QStringLiteral("/tmp/northstar-text-editor-missing.txt")));
    QVERIFY(controller.statusMessage().contains(QStringLiteral("no longer available")));
}

void TextEditorControllerTest::savesNewDocument()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("new-document.txt"));

    TextEditorController controller;
    controller.setText(QStringLiteral("A new Northstar document"));
    QVERIFY(controller.dirty());
    QVERIFY(controller.canSave());
    QVERIFY(controller.saveAs(path));
    QCOMPARE(controller.filePath(), QFileInfo(path).absoluteFilePath());

    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(saved.readAll(), QByteArray("A new Northstar document"));
    QVERIFY(!controller.dirty());
}

QTEST_MAIN(TextEditorControllerTest)
#include "test-texteditorcontroller.moc"
