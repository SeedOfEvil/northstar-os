#include "texteditorcontroller.h"
#include "northstarappearance.h"
#include "northstarui.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QUrl>

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

bool fileContains(const QString &path, const QByteArray &expected)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && file.readAll() == expected;
}

// Exercises the product behavior a headless FreeBSD gate can prove: multiple
// documents, saving, recent-file persistence, find/replace, and the explicit
// refusals for documents that cannot be edited as UTF-8 text.
int runSelfTest()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        return 1;
    }
    const QString recentPath = temporaryDirectory.filePath(QStringLiteral("recent.ini"));
    const QString notesPath = temporaryDirectory.filePath(QStringLiteral("notes.txt"));
    const QString binaryPath = temporaryDirectory.filePath(QStringLiteral("binary.txt"));
    const QString newPath = temporaryDirectory.filePath(QStringLiteral("new-document.txt"));

    if (!writeFile(notesPath, "Northstar")
        || !writeFile(binaryPath, QByteArray("north\0star", 10))) {
        return 1;
    }

    {
        TextEditorController controller(nullptr, recentPath);
        if (controller.documentCount() != 1 || !controller.untitled()) {
            return 1;
        }

        if (!controller.openFile(notesPath) || controller.text() != QStringLiteral("Northstar")) {
            return 1;
        }
        if (controller.documentCount() != 1) {
            return 1; // the pristine untitled tab is reused, not stacked
        }

        controller.setText(QStringLiteral("Northstar editor"));
        if (!controller.dirty() || !controller.save() || controller.dirty()) {
            return 1;
        }
        if (!fileContains(notesPath, "Northstar editor")) {
            return 1;
        }

        // A second document opens beside the first instead of replacing it.
        controller.newDocument();
        controller.setText(QStringLiteral("A new Northstar document"));
        if (controller.documentCount() != 2 || !controller.saveAs(newPath)) {
            return 1;
        }
        if (!fileContains(newPath, "A new Northstar document")) {
            return 1;
        }

        // Find and replace operates on the active document.
        controller.setFindQuery(QStringLiteral("Northstar"));
        if (controller.matchCount() != 1 || !controller.findNext()) {
            return 1;
        }
        controller.setReplacementText(QStringLiteral("Lunar"));
        if (controller.replaceAll() != 1
            || controller.text() != QStringLiteral("A new Lunar document")) {
            return 1;
        }

        // Unsupported and missing documents are refused with a stated reason.
        if (controller.openFile(binaryPath) || !controller.statusIsError()
            || !controller.statusMessage().contains(QStringLiteral("UTF-8"))) {
            return 1;
        }
        if (controller.openFile(temporaryDirectory.filePath(QStringLiteral("absent.txt")))) {
            return 1;
        }

        // A dirty tab refuses a plain close; discarding it always succeeds.
        controller.setText(QStringLiteral("unsaved"));
        if (controller.closeDocument(controller.activeIndex())) {
            return 1;
        }
        controller.discardDocument(controller.activeIndex());
        if (controller.documentCount() != 1 || controller.anyDirty()) {
            return 1;
        }
    }

    // The recent-file history survives a restart of the application.
    {
        TextEditorController restarted(nullptr, recentPath);
        if (!restarted.hasRecentFiles() || restarted.recentFiles().size() != 2) {
            return 1;
        }
        const QVariantMap newest = restarted.recentFiles().first().toMap();
        if (newest.value(QStringLiteral("path")).toString()
            != QFileInfo(newPath).absoluteFilePath()) {
            return 1;
        }
    }

    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-text-editor"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption selfTestOption(QStringLiteral("self-test"),
                                       QStringLiteral("validate document, history, and find behavior without opening a window"));
    QCommandLineOption qmlSelfTestOption(QStringLiteral("qml-self-test"),
                                          QStringLiteral("load the Text Editor QML surface without entering the event loop"));
    parser.addOption(selfTestOption);
    parser.addOption(qmlSelfTestOption);
    parser.addPositionalArgument(QStringLiteral("file"),
                                 QStringLiteral("text file to open"),
                                 QStringLiteral("[file...]"));
    parser.process(application);

    if (parser.isSet(selfTestOption)) {
        return runSelfTest();
    }

    TextEditorController controller;
    bool openedAnyFile = false;
    for (const QString &argument : parser.positionalArguments()) {
        openedAnyFile = controller.openFile(argument) || openedAnyFile;
    }
    if (!parser.positionalArguments().isEmpty() && !openedAnyFile) {
        // Files and the desktop launch the editor for one document at a time.
        // Reporting the failure on stderr keeps the launch log truthful instead
        // of leaving an empty window that looks like a successful open.
        qWarning("%s", qPrintable(controller.statusMessage()));
    }

    NorthstarUi::registerTypes();
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("northstarTextEditorController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("northstarDarkMode"),
                                             NorthstarAppearance::darkMode());
    engine.load(QUrl(QStringLiteral("qrc:/Northstar/TextEditor/TextEditorWindow.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (parser.isSet(qmlSelfTestOption)) {
        return 0;
    }

    return application.exec();
}
