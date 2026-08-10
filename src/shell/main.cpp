#include "applicationlauncher.h"
#include "desktopitemscontroller.h"
#include "desktoplayoutcontroller.h"
#include "filebrowsercontroller.h"
#include "layershellsurface.h"
#include "notificationcenter.h"
#include "packagecatalog.h"
#include "packagetrustcontroller.h"
#include "pinnedapplicationmodel.h"
#include "powercontroller.h"
#include "searchcontroller.h"
#include "sessioncontroller.h"
#include "shellstate.h"
#include "shortcutcatalog.h"
#include "updateauthorizationcontroller.h"
#include "updateplancontroller.h"
#include "volumecatalog.h"
#include "windowcontroller.h"
#include "northstarui.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QStandardPaths>
#include <QVariant>
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

QUrl northstarIconsSource()
{
    const QString path = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("northstar/icons/northstar-icons.png"),
        QStandardPaths::LocateFile);
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

QUrl northstarGeneratedIconsDirectory()
{
    const QString path = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("northstar/icons/generated"),
        QStandardPaths::LocateDirectory);
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path + QLatin1Char('/'));
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("northstar-shell"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    NorthstarUi::registerTypes();
    QQmlApplicationEngine engine;
    ShellState shellState;
    ApplicationLauncher applicationLauncher;
    NotificationCenter notificationCenter;
    PackageCatalog packageCatalog;
    PackageTrustController packageTrustController;
    PinnedApplicationModel pinnedApplicationModel;
    UpdatePlanController updatePlanController(&packageTrustController);
    UpdateAuthorizationController updateAuthorizationController(&packageTrustController,
                                                                  &updatePlanController);
    DesktopItemsController desktopItemsController;
    FileBrowserController fileBrowserController;
    DesktopLayoutController desktopLayoutController;
    PowerController powerController;
    SearchController searchController(&applicationLauncher);
    SessionController sessionController;
    ShortcutCatalog shortcutCatalog;
    VolumeController volumeController;
    WindowController windowController;
    QObject::connect(&applicationLauncher, &ApplicationLauncher::launchStatusChanged,
                     &notificationCenter, [&applicationLauncher, &notificationCenter]() {
        if (applicationLauncher.launchMessage().isEmpty()) {
            return;
        }

        notificationCenter.pushNotification(
            applicationLauncher.lastLaunchSucceeded()
                ? QStringLiteral("Application started")
                : QStringLiteral("Application launch failed"),
            applicationLauncher.launchMessage(),
            applicationLauncher.lastLaunchSucceeded()
                ? QStringLiteral("success")
                : QStringLiteral("error"));
    });
    const QUrl logoSource = northstarLogoSource();
    const QUrl iconsSource = northstarIconsSource();
    const QUrl generatedIconsDirectory = northstarGeneratedIconsDirectory();
    engine.rootContext()->setContextProperty(QStringLiteral("northstarGeneratedIconsDirectory"),
                                             generatedIconsDirectory);
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
        backgroundContext->setContextProperty(QStringLiteral("northstarIconsSource"), iconsSource);
        backgroundContext->setContextProperty(QStringLiteral("northstarDesktopItemsController"),
                                              &desktopItemsController);
        backgroundContext->setContextProperty(QStringLiteral("northstarFileBrowserController"),
                                              &fileBrowserController);
        backgroundContext->setContextProperty(QStringLiteral("northstarGeneratedIconsDirectory"), generatedIconsDirectory);
        backgroundContext->setContextProperty(QStringLiteral("northstarDesktopLayoutController"), &desktopLayoutController);
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
        context->setContextProperty(QStringLiteral("northstarNotificationCenter"), &notificationCenter);
        context->setContextProperty(QStringLiteral("northstarPackageCatalog"), &packageCatalog);
        context->setContextProperty(QStringLiteral("northstarPackageTrustController"), &packageTrustController);
        context->setContextProperty(QStringLiteral("northstarUpdatePlanController"), &updatePlanController);
        context->setContextProperty(QStringLiteral("northstarUpdateAuthorizationController"),
                                    &updateAuthorizationController);
        context->setContextProperty(QStringLiteral("northstarDesktopLayoutController"), &desktopLayoutController);
        context->setContextProperty(QStringLiteral("northstarFileBrowserController"), &fileBrowserController);
        context->setContextProperty(QStringLiteral("northstarDesktopItemsController"),
                                    &desktopItemsController);
        context->setContextProperty(QStringLiteral("northstarPowerController"), &powerController);
        context->setContextProperty(QStringLiteral("northstarSearchController"), &searchController);
        context->setContextProperty(QStringLiteral("northstarSessionController"), &sessionController);
        context->setContextProperty(QStringLiteral("northstarShortcutCatalog"), &shortcutCatalog);
        context->setContextProperty(QStringLiteral("northstarVolumeController"), &volumeController);
        context->setContextProperty(QStringLiteral("northstarWindowController"), &windowController);
        context->setContextProperty(QStringLiteral("northstarPinnedApplicationModel"),
                                    &pinnedApplicationModel);
        context->setContextProperty(QStringLiteral("northstarLogoSource"), logoSource);
        context->setContextProperty(QStringLiteral("northstarIconsSource"), iconsSource);
        context->setContextProperty(QStringLiteral("northstarGeneratedIconsDirectory"), generatedIconsDirectory);
        context->setContextProperty(QStringLiteral("targetScreen"), screen);
        context->setContextProperty(QStringLiteral("displayIndex"), index);

        QObject *object = component.create(context);
        if (backgroundObject != nullptr && object != nullptr) {
            auto *filesWindow = object->findChild<QObject *>(QStringLiteral("fileBrowserWindow"));
            backgroundObject->setProperty("fileBrowserWindow", QVariant::fromValue(filesWindow));
        }
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
        dockContext->setContextProperty(QStringLiteral("northstarVolumeController"), &volumeController);
        dockContext->setContextProperty(QStringLiteral("northstarWindowController"), &windowController);
        dockContext->setContextProperty(QStringLiteral("pinnedApplicationModel"), &pinnedApplicationModel);
        dockContext->setContextProperty(QStringLiteral("northstarLogoSource"), logoSource);
        dockContext->setContextProperty(QStringLiteral("northstarIconsSource"), iconsSource);
        dockContext->setContextProperty(QStringLiteral("northstarGeneratedIconsDirectory"), generatedIconsDirectory);
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
