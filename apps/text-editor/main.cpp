#include "texteditorcontroller.h"
#include "northstarappearance.h"
#include "northstarui.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-text-editor"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption selfTestOption(QStringLiteral("self-test"),
                                       QStringLiteral("validate text loading and saving without opening a window"));
    QCommandLineOption qmlSelfTestOption(QStringLiteral("qml-self-test"),
                                          QStringLiteral("load the Text Editor QML surface without entering the event loop"));
    parser.addOption(selfTestOption);
    parser.addOption(qmlSelfTestOption);
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("text file to open"));
    parser.process(application);

    const bool selfTest = parser.isSet(selfTestOption);
    if (selfTest) {
        QTemporaryDir temporaryDirectory;
        if (!temporaryDirectory.isValid()) {
            return 1;
        }
        const QString path = temporaryDirectory.filePath(QStringLiteral("self-test.txt"));
        QFile source(path);
        if (!source.open(QIODevice::WriteOnly) || source.write("Northstar") != 9) {
            return 1;
        }
        source.close();

        TextEditorController controller;
        if (!controller.loadFile(path) || controller.text() != QStringLiteral("Northstar")) {
            return 1;
        }
        controller.setText(QStringLiteral("Northstar editor"));
        if (!controller.dirty() || !controller.save()) {
            return 1;
        }
        QFile saved(path);
        if (!saved.open(QIODevice::ReadOnly) || saved.readAll() != "Northstar editor") {
            return 1;
        }

        const QString newPath = temporaryDirectory.filePath(QStringLiteral("new-document.txt"));
        TextEditorController newDocument;
        newDocument.setText(QStringLiteral("Northstar new document"));
        if (!newDocument.canSave() || !newDocument.saveAs(newPath)) {
            return 1;
        }
        QFile newSaved(newPath);
        if (!newSaved.open(QIODevice::ReadOnly)
            || newSaved.readAll() != "Northstar new document") {
            return 1;
        }
        return 0;
    }

    TextEditorController controller;
    const QStringList positionalArguments = parser.positionalArguments();
    if (!positionalArguments.isEmpty() && !controller.loadFile(positionalArguments.first())) {
        return 1;
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
