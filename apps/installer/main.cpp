#include "installercontroller.h"
#include "installerrecoverycontroller.h"
#include "northstarappearance.h"
#include "northstarui.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-installer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(NORTHSTAR_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Northstar installation assistant"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption selfTest(QStringLiteral("self-test"), QStringLiteral("load the installer surface without disk discovery"));
    parser.addOption(selfTest);
    parser.process(application);

    NorthstarUi::registerTypes();
    InstallerController controller;
    InstallerRecoveryController recoveryController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("installerController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("installerRecoveryController"), &recoveryController);
    engine.rootContext()->setContextProperty(QStringLiteral("northstarDarkMode"), NorthstarAppearance::darkMode());
    engine.rootContext()->setContextProperty(QStringLiteral("installerSelfTest"), parser.isSet(selfTest));
    engine.load(QUrl(QStringLiteral("qrc:/Northstar/Installer/InstallerWindow.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return parser.isSet(selfTest) ? 0 : application.exec();
}
