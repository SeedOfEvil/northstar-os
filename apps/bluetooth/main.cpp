#include "bluetoothcontroller.h"
#include "northstarappearance.h"
#include "northstarui.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-bluetooth"));
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption selfTest(QStringLiteral("self-test"),
                                QStringLiteral("Load the Bluetooth surface without scanning."));
    parser.addOption(selfTest);
    parser.process(app);

    NorthstarUi::registerTypes();
    BluetoothController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bluetoothController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("northstarDarkMode"), NorthstarAppearance::darkMode());
    engine.rootContext()->setContextProperty(QStringLiteral("bluetoothSelfTest"), parser.isSet(selfTest));
    engine.load(QUrl(QStringLiteral("qrc:/Northstar/Bluetooth/BluetoothWindow.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return parser.isSet(selfTest) ? 0 : app.exec();
}
