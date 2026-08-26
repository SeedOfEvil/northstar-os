#include "northstarappearance.h"
#include "northstarui.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QSysInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-welcome"));
    QCoreApplication::setApplicationVersion(QStringLiteral(NORTHSTAR_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Northstar's first-run welcome surface"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption selfTestOption(QStringLiteral("self-test"),
                                       QStringLiteral("validate the installed Welcome application without opening a window"));
    parser.addOption(selfTestOption);
    parser.process(application);

    NorthstarUi::registerTypes();
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("northstarHomePath"), QDir::homePath());
    engine.rootContext()->setContextProperty(QStringLiteral("northstarDarkMode"),
                                             NorthstarAppearance::darkMode());
    const QString waylandDisplay = qEnvironmentVariable("WAYLAND_DISPLAY");
    const QString x11Display = qEnvironmentVariable("DISPLAY");
    const QString sessionStatus = !waylandDisplay.isEmpty()
        ? QStringLiteral("Wayland (%1)").arg(waylandDisplay)
        : (!x11Display.isEmpty()
            ? QStringLiteral("X11 (%1)").arg(x11Display)
            : QStringLiteral("Session display not detected"));
    engine.rootContext()->setContextProperty(QStringLiteral("northstarVersion"),
                                             application.applicationVersion());
    engine.rootContext()->setContextProperty(QStringLiteral("northstarBuild"),
                                             QStringLiteral("development"));
    engine.rootContext()->setContextProperty(QStringLiteral("northstarSessionStatus"),
                                             sessionStatus);
    engine.rootContext()->setContextProperty(QStringLiteral("northstarPlatform"),
                                             QSysInfo::prettyProductName());
    engine.load(QUrl(QStringLiteral("qrc:/Northstar/Welcome/WelcomeWindow.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (parser.isSet(selfTestOption)) {
        return 0;
    }

    return application.exec();
}
