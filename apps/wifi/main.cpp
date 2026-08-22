#include "wificontroller.h"
#include "northstarappearance.h"
#include "northstarui.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-wifi"));
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption selfTest(QStringLiteral("self-test"), QStringLiteral("Load the Wi-Fi surface without scanning."));
    parser.addOption(selfTest);
    parser.process(app);

    NorthstarUi::registerTypes();
    WifiController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("wifiController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("northstarDarkMode"), NorthstarAppearance::darkMode());
    engine.rootContext()->setContextProperty(QStringLiteral("wifiSelfTest"), parser.isSet(selfTest));
    engine.load(QUrl(QStringLiteral("qrc:/Northstar/Wifi/WifiWindow.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return parser.isSet(selfTest) ? 0 : app.exec();
}
