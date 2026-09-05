#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Northstar Packaging Demo");
    QQmlApplicationEngine engine;
    engine.loadData(R"(
import QtQuick
import QtQuick.Window
Window {
    visible: true
    width: 480; height: 220
    title: "Northstar Packaging Demo"
    color: "#142536"
    Text {
        anchors.centerIn: parent
        width: parent.width - 48
        text: "Your packaged app is running.\n\nInstalled by Northstar, launched as you."
        color: "#e6f4ff"
        font.pixelSize: 20
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
)");
    if (engine.rootObjects().isEmpty())
        return 1;
    if (app.arguments().contains("--self-test"))
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    return app.exec();
}
