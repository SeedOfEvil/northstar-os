#include "applicationlauncher.h"
#include "layershellsurface.h"
#include "sessioncontroller.h"
#include "shellstate.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QUrl>
#include <QWindow>

namespace {

constexpr int PanelHeight = 96;

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-shell"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QQmlApplicationEngine engine;
    ShellState shellState;
    ApplicationLauncher applicationLauncher;
    SessionController sessionController;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/Northstar/Shell/ShellWindow.qml")));

    if (component.status() == QQmlComponent::Error) {
        for (const auto &error : component.errors()) {
            qCritical().noquote() << error.toString();
        }
        return 1;
    }

    QList<QObject *> surfaces;
    QList<QQmlContext *> contexts;
    int displayIndex = 0;

    const auto createSurface = [&](QScreen *screen, int index) -> bool {
        auto *context = new QQmlContext(engine.rootContext());
        context->setContextProperty(QStringLiteral("shellState"), &shellState);
        context->setContextProperty(QStringLiteral("launcher"), &applicationLauncher);
        context->setContextProperty(QStringLiteral("sessionController"), &sessionController);
        context->setContextProperty(QStringLiteral("targetScreen"), screen);
        context->setContextProperty(QStringLiteral("displayIndex"), index);

        QObject *object = component.create(context);
        auto *window = qobject_cast<QWindow *>(object);
        if (window == nullptr || !LayerShellSurface::configurePanel(window, screen, PanelHeight, index)) {
            qWarning() << "Unable to configure Northstar shell surface for display" << index;
            delete object;
            delete context;
            return false;
        }

        surfaces.append(object);
        contexts.append(context);
        window->show();
        return true;
    };

    for (QScreen *screen : application.screens()) {
        if (!createSurface(screen, displayIndex)) {
            return 1;
        }
        ++displayIndex;
    }

    if (surfaces.isEmpty()) {
        qCritical() << "No connected display was available for the Northstar shell";
        return 1;
    }

    Q_UNUSED(contexts);
    return application.exec();
}
