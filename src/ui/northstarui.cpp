#include "northstarui.h"

#include <QQmlEngine>
#include <QUrl>

void NorthstarUi::registerTypes()
{
    qmlRegisterType(QUrl(QStringLiteral("qrc:/Northstar/Ui/LunarPalette.qml")),
                    "Northstar.Ui", 1, 0, "LunarPalette");
    qmlRegisterType(QUrl(QStringLiteral("qrc:/Northstar/Ui/NativeWindowMoveHandler.qml")),
                    "Northstar.Ui", 1, 0, "NativeWindowMoveHandler");
    qmlRegisterType(QUrl(QStringLiteral("qrc:/Northstar/Ui/NativeWindowResizeHandler.qml")),
                    "Northstar.Ui", 1, 0, "NativeWindowResizeHandler");
    qmlRegisterType(QUrl(QStringLiteral("qrc:/Northstar/Ui/NorthstarIcon.qml")),
                    "Northstar.Ui", 1, 0, "NorthstarIcon");
    qmlRegisterType(QUrl(QStringLiteral("qrc:/Northstar/Ui/NorthstarWindowControls.qml")),
                    "Northstar.Ui", 1, 0, "NorthstarWindowControls");
    qmlRegisterType(QUrl(QStringLiteral("qrc:/Northstar/Ui/NorthstarWindowFrame.qml")),
                    "Northstar.Ui", 1, 0, "NorthstarWindowFrame");
    qmlRegisterType(QUrl(QStringLiteral("qrc:/Northstar/Ui/NorthstarWindowTitleBar.qml")),
                    "Northstar.Ui", 1, 0, "NorthstarWindowTitleBar");
}
