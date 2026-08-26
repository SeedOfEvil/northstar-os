#include "applicationlauncher.h"
#include "clockcontroller.h"
#include "desktopitemscontroller.h"
#include "desktoplayoutcontroller.h"
#include "desktopsettings.h"
#include "filebrowsercontroller.h"
#include "inputcontroller.h"
#include "layershellsurface.h"
#include "notificationcenter.h"
#include "packagecatalog.h"
#include "packagemutationcontroller.h"
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
#include "wallpapercontroller.h"
#include "windowcontroller.h"
#include "northstarui.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QVariant>
#include <QUrl>
#include <QWindow>

namespace {

constexpr int PanelHeight = 46;
// The dock is a floating surface with a deliberate bottom gap. Layer-shell
// owns the outer window size, so it must include both the 88 px glass surface
// and its 22 px inset instead of retaining the legacy 72 px allocation.
constexpr int DockHeight = 126;

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
        QStringLiteral("northstar/icons/northstar-icons-aurora.png"),
        QStandardPaths::LocateFile);
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

QUrl northstarAuroraWallpaperSource()
{
    const QString path = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("northstar/wallpapers/aurora-glass.png"),
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
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    QGuiApplication application(argc, argv);
    const QStringList preferredFonts{QStringLiteral("Noto Sans"),
                                     QStringLiteral("DejaVu Sans"),
                                     QStringLiteral("Liberation Sans")};
    QString family = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    const QStringList installedFonts = QFontDatabase::families();
    for (const QString &candidate : preferredFonts) {
        if (installedFonts.contains(candidate)) {
            family = candidate;
            break;
        }
    }
    QFont interfaceFont(family);
    interfaceFont.setPointSize(10);
    application.setFont(interfaceFont);
    // A DRM resume briefly removes the physical output and gives Qt a
    // placeholder screen. That must not turn a successful S3 cycle into a
    // clean shell exit, which the session supervisor correctly treats as a
    // user logout.
    application.setQuitOnLastWindowClosed(false);
    const bool qmlSelfTest =
        application.arguments().contains(QStringLiteral("--qml-self-test"));
    QCoreApplication::setApplicationName(QStringLiteral("northstar-shell"));
    QCoreApplication::setApplicationVersion(QStringLiteral(NORTHSTAR_VERSION_STRING));

    NorthstarUi::registerTypes();
    QQmlApplicationEngine engine;

    // A QML binding error is invisible in a normal session but means a surface
    // silently rendered nothing. The self-test treats any of them as failure.
    int qmlWarnings = 0;
    if (qmlSelfTest) {
        QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                         [&qmlWarnings](const QList<QQmlError> &reported) {
            for (const QQmlError &error : reported) {
                qmlWarnings += 1;
                qWarning().noquote() << QStringLiteral("QML warning: %1").arg(error.toString());
            }
        });
    }
    ShellState shellState;
    ApplicationLauncher applicationLauncher;
    ClockController clockController;
    NotificationCenter notificationCenter;
    QuickSettingsController quickSettingsController;
    InputController inputController;
    PackageCatalog packageCatalog;
    PackageMutationController packageMutationController(&packageCatalog);
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
    WallpaperController wallpaperController;
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
    bool lowBatteryNotified = false;
    const auto refreshBatteryNotification = [&powerController, &notificationCenter,
                                             &lowBatteryNotified]() {
        const bool low = powerController.batteryAvailable()
            && !powerController.onAcPower() && powerController.batteryPercentage() <= 15;
        if (low && !lowBatteryNotified) {
            notificationCenter.pushNotification(
                QStringLiteral("Low battery"),
                QStringLiteral("Connect the power adapter. %1% remains.")
                    .arg(powerController.batteryPercentage()),
                QStringLiteral("warning"));
            lowBatteryNotified = true;
        } else if (!low && (powerController.onAcPower()
                            || powerController.batteryPercentage() >= 20)) {
            lowBatteryNotified = false;
        }
    };
    QObject::connect(&powerController, &PowerController::batteryChanged,
                     &notificationCenter, refreshBatteryNotification);
    refreshBatteryNotification();
    SettingsCatalog settingsCatalog;
    registerDesktopSettings(&settingsCatalog,
                            &shellState,
                            &quickSettingsController,
                            &inputController,
                            &notificationCenter,
                            &desktopLayoutController,
                            &applicationLauncher,
                            &pinnedApplicationModel,
                            &powerController,
                            &sessionController,
                            &wallpaperController,
                            &clockController);

    // Settings reads straight through to the controllers, so it only has to be
    // told when one of them changed underneath it.
    const auto refreshSettings = [&settingsCatalog]() { settingsCatalog.refresh(); };
    QObject::connect(&shellState, &ShellState::darkModeChanged, &settingsCatalog, refreshSettings);
    QObject::connect(&shellState, &ShellState::filesGridViewChanged, &settingsCatalog, refreshSettings);
    QObject::connect(&wallpaperController, &WallpaperController::wallpaperChanged, &settingsCatalog,
                     refreshSettings);
    QObject::connect(&clockController, &ClockController::clockChanged, &settingsCatalog,
                     refreshSettings);
    QObject::connect(&quickSettingsController, &QuickSettingsController::capabilitiesChanged,
                     &settingsCatalog, refreshSettings);
    QObject::connect(&inputController, &InputController::changed,
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
    QObject::connect(&powerController, &PowerController::batteryChanged,
                     &settingsCatalog, refreshSettings);
    QObject::connect(&powerController, &PowerController::powerCapabilitiesChanged,
                     &settingsCatalog, refreshSettings);

    const QUrl logoSource = northstarLogoSource();
    const QUrl iconsSource = northstarIconsSource();
    const QUrl auroraWallpaperSource = northstarAuroraWallpaperSource();
    const QUrl generatedIconsDirectory = northstarGeneratedIconsDirectory();
    engine.rootContext()->setContextProperty(QStringLiteral("northstarGeneratedIconsDirectory"),
                                             generatedIconsDirectory);
    QQmlComponent backgroundComponent(&engine, QUrl(QStringLiteral("qrc:/Northstar/Shell/DesktopBackground.qml")));
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/Northstar/Shell/ShellWindow.qml")));
    QQmlComponent dockComponent(&engine, QUrl(QStringLiteral("qrc:/Northstar/Shell/DockWindow.qml")));
    QQmlComponent displayConfirmationComponent(
        &engine, QUrl(QStringLiteral("qrc:/Northstar/Shell/DisplayModeConfirmationWindow.qml")));

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
    if (displayConfirmationComponent.status() == QQmlComponent::Error) {
        for (const auto &error : displayConfirmationComponent.errors()) {
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

    // The surfaces hold bindings onto every controller declared above, and
    // those controllers are destroyed when this function returns. Tearing the
    // surfaces down first stops their bindings re-evaluating against
    // half-destroyed objects, which otherwise fills shutdown with TypeErrors.
    //
    // This is a scope guard rather than a call at the end because it has to
    // run on the early failure returns too, and because a teardown that can be
    // forgotten is a teardown that eventually is. Declared here, after the
    // controllers, it is destroyed before them. Objects go before the contexts
    // they were created in.
    struct SurfaceTeardown
    {
        QList<QObject *> &surfaces;
        QList<QQmlContext *> &contexts;

        void operator()()
        {
            qDeleteAll(surfaces);
            surfaces.clear();
            qDeleteAll(contexts);
            contexts.clear();
        }

        ~SurfaceTeardown() { (*this)(); }
    } destroySurfaces{surfaces, contexts};

    // Unlike the panel/background/dock surfaces, display confirmation must
    // survive a compositor output rebuild. Otherwise a Keep/Revert click can
    // land on a window being torn down and appear to be ignored.
    QObject *displayConfirmationObject = nullptr;
    struct PersistentSurfaceTeardown
    {
        QObject *&object;
        ~PersistentSurfaceTeardown()
        {
            delete object;
            object = nullptr;
        }
    } destroyPersistentSurface{displayConfirmationObject};

    int displayIndex = 0;

    const auto createSurface = [&](QScreen *screen, int index) -> bool {
        auto *backgroundContext = new QQmlContext(engine.rootContext());
        backgroundContext->setContextProperty(QStringLiteral("northstarLogoSource"), logoSource);
        backgroundContext->setContextProperty(QStringLiteral("northstarIconsSource"), iconsSource);
        backgroundContext->setContextProperty(QStringLiteral("northstarAuroraWallpaperSource"),
                                              auroraWallpaperSource);
        backgroundContext->setContextProperty(QStringLiteral("northstarDesktopItemsController"),
                                              &desktopItemsController);
        backgroundContext->setContextProperty(QStringLiteral("northstarFileBrowserController"),
                                              &fileBrowserController);
        backgroundContext->setContextProperty(QStringLiteral("northstarPreviewController"),
                                              &previewController);
        backgroundContext->setContextProperty(QStringLiteral("northstarGeneratedIconsDirectory"), generatedIconsDirectory);
        backgroundContext->setContextProperty(QStringLiteral("northstarDesktopLayoutController"), &desktopLayoutController);
        backgroundContext->setContextProperty(QStringLiteral("shellState"), &shellState);
        backgroundContext->setContextProperty(QStringLiteral("northstarWallpaperController"),
                                              &wallpaperController);
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
        context->setContextProperty(QStringLiteral("northstarPackageMutationController"),
                                    &packageMutationController);
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
        context->setContextProperty(QStringLiteral("northstarWallpaperController"),
                                    &wallpaperController);
        context->setContextProperty(QStringLiteral("northstarClockController"), &clockController);
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

    QVariantMap displayConfirmationProperties;
    displayConfirmationProperties.insert(
        QStringLiteral("controller"),
        QVariant::fromValue(static_cast<QObject *>(&quickSettingsController)));
    displayConfirmationProperties.insert(
        QStringLiteral("state"),
        QVariant::fromValue(static_cast<QObject *>(&shellState)));
    displayConfirmationProperties.insert(
        QStringLiteral("targetScreen"),
        QVariant::fromValue(static_cast<QObject *>(application.primaryScreen())));
    displayConfirmationProperties.insert(QStringLiteral("panelHeight"), PanelHeight);
    displayConfirmationObject = displayConfirmationComponent.createWithInitialProperties(
        displayConfirmationProperties, engine.rootContext());
    if (displayConfirmationObject == nullptr) {
        for (const auto &error : displayConfirmationComponent.errors()) {
            qCritical().noquote() << error.toString();
        }
        return 1;
    }
    const auto updateConfirmationScreen = [displayConfirmationObject](QScreen *screen) {
        if (screen != nullptr) {
            displayConfirmationObject->setProperty(
                "targetScreen", QVariant::fromValue(static_cast<QObject *>(screen)));
        }
    };
    QObject::connect(&application, &QGuiApplication::primaryScreenChanged,
                     displayConfirmationObject, updateConfirmationScreen);

    if (qmlSelfTest) {
        const int selfTestStatus = runShellSelfTest(surfaces);

        // Run the teardown early rather than leaving it to the scope guard, so
        // any binding error it raises is counted before the check below.
        delete displayConfirmationObject;
        displayConfirmationObject = nullptr;
        destroySurfaces();
        // Counted after teardown so binding errors raised on the way down are
        // included. Anything emitted later than this cannot be seen from here,
        // which is why the teardown above is a scope guard and not a gate.
        if (qmlWarnings > 0) {
            qCritical().noquote()
                << QStringLiteral("the shell self-test produced %1 QML warning(s)").arg(qmlWarnings);
            return 1;
        }
        return selfTestStatus;
    }

    // wlroots temporarily replaces the DRM output with a headless NOOP output
    // while the seat is paused for S3. Qt moves the existing windows onto its
    // placeholder screen, but does not move their layer-shell bindings back
    // when eDP-1 returns. Debounce the burst of screenRemoved/screenAdded
    // signals and rebuild only Northstar's shell surfaces after the output set
    // has settled. Application windows remain owned by the compositor.
    bool controllerModesetInProgress = false;
    QTimer controllerModesetGuard;
    controllerModesetGuard.setSingleShot(true);
    controllerModesetGuard.setInterval(2000);
    QObject::connect(&controllerModesetGuard, &QTimer::timeout,
                     &application, [&controllerModesetInProgress]() {
        controllerModesetInProgress = false;
    });

    QTimer outputRefreshTimer;
    outputRefreshTimer.setSingleShot(true);
    outputRefreshTimer.setInterval(750);
    QObject::connect(&outputRefreshTimer, &QTimer::timeout, &application, [&]() {
        bool reopenSettings = false;
        for (QObject *surface : std::as_const(surfaces)) {
            QObject *settingsWindow = surface != nullptr
                ? surface->findChild<QObject *>(QStringLiteral("settingsWindow"))
                : nullptr;
            reopenSettings = reopenSettings
                || (settingsWindow != nullptr
                    && settingsWindow->property("visible").toBool());
        }
        destroySurfaces();
        displayIndex = 0;
        for (QScreen *screen : application.screens()) {
            if (!createSurface(screen, displayIndex)) {
                qWarning() << "Unable to rebuild Northstar surfaces after an output change";
                destroySurfaces();
                return;
            }
            ++displayIndex;
        }
        if (surfaces.isEmpty()) {
            qWarning() << "No connected display was available after an output change";
        } else {
            qInfo() << "Northstar surfaces rebuilt after output change";
            if (quickSettingsController.displayModePending()) {
                QMetaObject::invokeMethod(displayConfirmationObject,
                                          "remapIfPending",
                                          Qt::QueuedConnection);
            }
            if (reopenSettings) {
                for (QObject *surface : std::as_const(surfaces)) {
                    QObject *settingsWindow = surface != nullptr
                        ? surface->findChild<QObject *>(QStringLiteral("settingsWindow"))
                        : nullptr;
                    if (settingsWindow != nullptr) {
                        QMetaObject::invokeMethod(settingsWindow, "openSettings",
                                                  Qt::QueuedConnection);
                        break;
                    }
                }
            }
        }
    });
    const auto scheduleOutputRefresh = [&outputRefreshTimer,
                                        &controllerModesetInProgress](QScreen *) {
        if (controllerModesetInProgress) {
            return;
        }
        outputRefreshTimer.start();
    };
    QObject::connect(&application, &QGuiApplication::screenAdded,
                     &application, scheduleOutputRefresh);
    QObject::connect(&application, &QGuiApplication::screenRemoved,
                     &application, scheduleOutputRefresh);
    // A controller-driven mode change keeps the same physical connector.
    // Qt and layer-shell resize those live windows in place; running the S3
    // recovery path here destroyed Settings and remapped the confirmation
    // window unnecessarily. Suppress the screen-signal burst only around a
    // successful Northstar modeset. Genuine output loss still takes the full
    // rebuild path above.
    QObject::connect(&quickSettingsController,
                     &QuickSettingsController::displayModeApplied,
                     &application, [&outputRefreshTimer,
                                    &controllerModesetInProgress,
                                    &controllerModesetGuard]() {
        controllerModesetInProgress = true;
        outputRefreshTimer.stop();
        controllerModesetGuard.start();
    });
    QTimer::singleShot(0, &quickSettingsController, [&quickSettingsController]() {
        quickSettingsController.restorePersistedCustomDisplayMode();
    });

    // The scope guard tears the surfaces down on the way out of here.
    return application.exec();
}
