#include "applicationlauncher.h"
#include "desktopitemscontroller.h"
#include "desktoplayoutcontroller.h"
#include "desktopsettings.h"
#include "filebrowsercontroller.h"
#include "layershellsurface.h"
#include "notificationcenter.h"
#include "packagecatalog.h"
#include "packagetrustcontroller.h"
#include "pinnedapplicationmodel.h"
#include "powercontroller.h"
#include "previewcontroller.h"
#include "quicksettingscontroller.h"
#include "searchcontroller.h"
#include "sessioncontroller.h"
#include "settingscatalog.h"
#include "shellcommandserver.h"
#include "shellfocus.h"
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

bool expectVisible(QObject *overlay, bool expected, const char *stage)
{
    const bool actual = overlay->property("visible").toBool();
    if (actual != expected) {
        qCritical().noquote()
            << QStringLiteral("search overlay visible=%1 after %2, expected %3")
                   .arg(actual ? QStringLiteral("true") : QStringLiteral("false"),
                        QString::fromLatin1(stage),
                        expected ? QStringLiteral("true") : QStringLiteral("false"));
        return false;
    }
    return true;
}

// Headless check of the shell surfaces that a contract grep cannot reach.
// Unified search must reopen after it has been closed, not only once per shell
// start, so the open/close/open cycle is driven explicitly.
int runShellSelfTest(const QList<QObject *> &surfaces)
{
    QObject *overlay = nullptr;
    for (QObject *surface : surfaces) {
        overlay = surface->findChild<QObject *>(QStringLiteral("searchOverlay"));
        if (overlay != nullptr) {
            break;
        }
    }
    if (overlay == nullptr) {
        qCritical() << "the shell surface exposes no searchOverlay object";
        return 1;
    }

    const auto open = [overlay]() {
        return QMetaObject::invokeMethod(overlay, "openSearch",
                                         Q_ARG(QVariant, QVariant(QString())));
    };
    const auto close = [overlay]() {
        return QMetaObject::invokeMethod(overlay, "closeSearch");
    };

    if (!open() || !expectVisible(overlay, true, "the first open")) {
        return 1;
    }
    if (!close() || !expectVisible(overlay, false, "close")) {
        return 1;
    }
    if (!open() || !expectVisible(overlay, true, "reopening after close")) {
        return 1;
    }
    close();
    return 0;
}

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
    const bool qmlSelfTest =
        application.arguments().contains(QStringLiteral("--qml-self-test"));
    QCoreApplication::setApplicationName(QStringLiteral("northstar-shell"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    NorthstarUi::registerTypes();
    QQmlApplicationEngine engine;
    ShellState shellState;
    ApplicationLauncher applicationLauncher;
    NotificationCenter notificationCenter;
    QuickSettingsController quickSettingsController;
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
    PreviewController previewController;
    SearchController searchController(&applicationLauncher);
    SessionController sessionController;
    ShortcutCatalog shortcutCatalog;
    VolumeController volumeController;
    WindowController windowController;
    notificationCenter.setDoNotDisturb(quickSettingsController.doNotDisturb());
    QObject::connect(&quickSettingsController, &QuickSettingsController::doNotDisturbChanged,
                     &notificationCenter, [&quickSettingsController, &notificationCenter]() {
        notificationCenter.setDoNotDisturb(quickSettingsController.doNotDisturb());
    });
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
    SettingsCatalog settingsCatalog;
    registerDesktopSettings(&settingsCatalog,
                            &shellState,
                            &quickSettingsController,
                            &notificationCenter,
                            &desktopLayoutController,
                            &applicationLauncher,
                            &pinnedApplicationModel,
                            &sessionController);

    // Settings reads straight through to the controllers, so it only has to be
    // told when one of them changed underneath it.
    const auto refreshSettings = [&settingsCatalog]() { settingsCatalog.refresh(); };
    QObject::connect(&shellState, &ShellState::darkModeChanged, &settingsCatalog, refreshSettings);
    QObject::connect(&shellState, &ShellState::filesGridViewChanged, &settingsCatalog, refreshSettings);
    QObject::connect(&quickSettingsController, &QuickSettingsController::capabilitiesChanged,
                     &settingsCatalog, refreshSettings);
    QObject::connect(&quickSettingsController, &QuickSettingsController::doNotDisturbChanged,
                     &settingsCatalog, refreshSettings);
    QObject::connect(&notificationCenter, &NotificationCenter::notificationsChanged,
                     &settingsCatalog, refreshSettings);
    QObject::connect(&sessionController, &SessionController::statusChanged,
                     &settingsCatalog, refreshSettings);
    QObject::connect(&applicationLauncher, &ApplicationLauncher::applicationsChanged,
                     &settingsCatalog, refreshSettings);
    QObject::connect(&pinnedApplicationModel, &PinnedApplicationModel::desktopIdsChanged,
                     &settingsCatalog, refreshSettings);

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

    ShellFocus shellFocus;

    // Shell shortcuts registered in QML only fire while a shell window holds
    // keyboard focus, which a layer-shell panel does not. A global shortcut is
    // bound in the compositor and arrives here instead.
    ShellCommandServer shellCommandServer;
    if (!shellCommandServer.start()) {
        qWarning().noquote() << QStringLiteral("Northstar shell control socket unavailable: %1")
                                    .arg(shellCommandServer.lastError());
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
        backgroundContext->setContextProperty(QStringLiteral("northstarPreviewController"),
                                              &previewController);
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
        context->setContextProperty(QStringLiteral("northstarPreviewController"), &previewController);
        context->setContextProperty(QStringLiteral("northstarQuickSettingsController"),
                                    &quickSettingsController);
        context->setContextProperty(QStringLiteral("northstarSearchController"), &searchController);
        context->setContextProperty(QStringLiteral("northstarSessionController"), &sessionController);
        context->setContextProperty(QStringLiteral("northstarSettingsCatalog"), &settingsCatalog);
        context->setContextProperty(QStringLiteral("northstarShellFocus"), &shellFocus);
        context->setContextProperty(QStringLiteral("northstarShellCommands"),
                                    &shellCommandServer);
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
            auto *quickLookWindow = object->findChild<QObject *>(QStringLiteral("quickLookWindow"));
            backgroundObject->setProperty("fileBrowserWindow", QVariant::fromValue(filesWindow));
            backgroundObject->setProperty("quickLookWindow", QVariant::fromValue(quickLookWindow));
        }
        auto *window = qobject_cast<QWindow *>(object);
        if (window != nullptr && index == 0) {
            shellFocus.setPanelWindow(window);
        }
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

    if (qmlSelfTest) {
        return runShellSelfTest(surfaces);
    }

    return application.exec();
}
