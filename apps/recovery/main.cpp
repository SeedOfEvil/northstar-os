#include "bootenvironmentcontroller.h"
#include "northstarappearance.h"
#include "northstarui.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-recovery"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption selfTest(QStringLiteral("self-test"),
                                QStringLiteral("load the recovery surface without reading system state"));
    parser.addOption(selfTest);
    parser.process(application);

    NorthstarUi::registerTypes();
    BootEnvironmentController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bootEnvironmentController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("northstarDarkMode"), NorthstarAppearance::darkMode());
    engine.rootContext()->setContextProperty(QStringLiteral("recoverySelfTest"), parser.isSet(selfTest));
    engine.load(QUrl(QStringLiteral("qrc:/Northstar/Recovery/RecoveryWindow.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return parser.isSet(selfTest) ? 0 : application.exec();
}
