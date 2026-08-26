#include "firstbootcontroller.h"
#include "northstarappearance.h"
#include "northstarui.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-first-boot"));
    QCoreApplication::setApplicationVersion(QStringLiteral(NORTHSTAR_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Northstar first-boot account setup"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption selfTest(QStringLiteral("self-test"),
                                QStringLiteral("validate the setup surface without changing the system"));
    parser.addOption(selfTest);
    parser.process(application);

    NorthstarUi::registerTypes();
    FirstBootController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("firstBootController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("northstarDarkMode"),
                                             NorthstarAppearance::darkMode());
    engine.load(QUrl(QStringLiteral("qrc:/Northstar/FirstBoot/FirstBootWindow.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return parser.isSet(selfTest) ? 0 : application.exec();
}
