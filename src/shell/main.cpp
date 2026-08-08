#include "applicationlauncher.h"
#include "filebrowsercontroller.h"
#include "layershellsurface.h"
#include "powercontroller.h"
#include "sessioncontroller.h"
#include "shellstate.h"
#include "windowcontroller.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QStandardPaths>
#include <QUrl>
#include <QWindow>

namespace {

constexpr int PanelHeight = 44;
constexpr int DockHeight = 72;

QUrl northstarLogoSource()
{
    const QString path = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("northstar/branding/northstar-logo.png"),
        QStandardPaths::LocateFile);
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-shell"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QQmlApplicationEngine engine;
    ShellState shellState;
    ApplicationLauncher applicationLauncher;
    FileBrowserController fileBrowserController;
    PowerController powerController;
    SessionController sessionController;
    WindowController windowController;
    const QUrl logoSource = northstarLogoSource();
    QQmlComponent backgroundComponent(&engine, QUrl(QStringLiteral("qrc:/Northstar/Shell/DesktopBackground.qml")));
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/Northstar/Shell/ShellWindow.qml")));
    QQmlComponent dockComponent(&engine, QUrl(QStringLiteral("qrc:/Northstar/Shell/DockWindow.qml")));

    if (backgroundComponent.status() == QQmlComponent::Error) {
        for (const auto &error : backgroundComponent.errors()) {
            qCritical().noquote() << error.toString();
        }
        return 1;
    }
    if (component.status() == QQmlComponent::Error) {
        for (const auto &error : component.errors()) {
            qCritical().noquote() << error.toString();
        }
        return 1;
    }
    if (dockComponent.status() == QQmlComponent::Error) {
        for (const auto &error : dockComponent.errors()) {
            qCritical().noquote() << error.toString();
        }
        return 1;
    }

    QList<QObject *> surfaces;
    QList<QQmlContext *> contexts;
    int displayIndex = 0;

    const auto createSurface = [&](QScreen *screen, int index) -> bool {
        auto *backgroundContext = new QQmlContext(engine.rootContext());
        backgroundContext->setContextProperty(QStringLiteral("northstarLogoSource"), logoSource);
        backgroundContext->setContextProperty(QStringLiteral("shellState"), &shellState);
        backgroundContext->setContextProperty(QStringLiteral("targetScreen"), screen);
        backgroundContext->setContextProperty(QStringLiteral("displayIndex"), index);

        QObject *backgroundObject = backgroundComponent.create(backgroundContext);
        auto *backgroundWindow = qobject_cast<QWindow *>(backgroundObject);
        if (backgroundWindow == nullptr
            || !LayerShellSurface::configureBackground(backgroundWindow, screen, index)) {
            qWarning() << "Unable to configure Northstar desktop background for display" << index;
            delete backgroundObject;
            delete backgroundContext;
            return false;
        }

        auto *context = new QQmlContext(engine.rootContext());
        context->setContextProperty(QStringLiteral("shellState"), &shellState);
        context->setContextProperty(QStringLiteral("launcher"), &applicationLauncher);
        context->setContextProperty(QStringLiteral("northstarFileBrowserController"), &fileBrowserController);
        context->setContextProperty(QStringLiteral("northstarPowerController"), &powerController);
        context->setContextProperty(QStringLiteral("northstarSessionController"), &sessionController);
        context->setContextProperty(QStringLiteral("northstarWindowController"), &windowController);
        context->setContextProperty(QStringLiteral("northstarLogoSource"), logoSource);
        context->setContextProperty(QStringLiteral("targetScreen"), screen);
        context->setContextProperty(QStringLiteral("displayIndex"), index);

        QObject *object = component.create(context);
        auto *window = qobject_cast<QWindow *>(object);
        if (window == nullptr || !LayerShellSurface::configurePanel(window, screen, PanelHeight, index)) {
            qWarning() << "Unable to configure Northstar shell surface for display" << index;
            delete object;
            delete context;
            delete backgroundObject;
            delete backgroundContext;
            return false;
        }

        auto *dockContext = new QQmlContext(engine.rootContext());
        dockContext->setContextProperty(QStringLiteral("shellState"), &shellState);
        dockContext->setContextProperty(QStringLiteral("launcher"), &applicationLauncher);
        dockContext->setContextProperty(QStringLiteral("northstarFileBrowserController"), &fileBrowserController);
        dockContext->setContextProperty(QStringLiteral("northstarWindowController"), &windowController);
        dockContext->setContextProperty(QStringLiteral("targetScreen"), screen);
        dockContext->setContextProperty(QStringLiteral("displayIndex"), index);

        QObject *dockObject = dockComponent.create(dockContext);
        auto *dockWindow = qobject_cast<QWindow *>(dockObject);
        if (dockWindow == nullptr || !LayerShellSurface::configureDock(dockWindow, screen, DockHeight, index)) {
            qWarning() << "Unable to configure Northstar dock surface for display" << index;
            delete dockObject;
            delete dockContext;
            delete object;
            delete context;
            delete backgroundObject;
            delete backgroundContext;
            return false;
        }

        surfaces.append(backgroundObject);
        contexts.append(backgroundContext);
        backgroundWindow->show();
        surfaces.append(object);
        contexts.append(context);
        window->show();
        surfaces.append(dockObject);
        contexts.append(dockContext);
        dockWindow->show();
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
