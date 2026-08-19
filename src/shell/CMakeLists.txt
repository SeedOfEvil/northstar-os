add_library(northstar-shell-core STATIC
    applicationlauncher.cpp
    clockcontroller.cpp
    desktopitemscontroller.cpp
    desktoplayoutcontroller.cpp
    desktopsettings.cpp
    filebrowsercontroller.cpp
    notificationcenter.cpp
    notificationstore.cpp
    packagecatalog.cpp
    packagetrustcontroller.cpp
    pinnedapplicationmodel.cpp
    powercontroller.cpp
    previewcontroller.cpp
    quicksettingscontroller.cpp
    searchcontroller.cpp
    sessioncontroller.cpp
    settingscatalog.cpp
    shellcommandserver.cpp
    shellstate.cpp
    shortcutcatalog.cpp
    updateauthorizationcontroller.cpp
    updateplancontroller.cpp
    volumecatalog.cpp
    wallpapercontroller.cpp
    windowcontroller.cpp
)

target_include_directories(northstar-shell-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(northstar-shell-core
    PUBLIC
        northstar-launcher-core
        Qt6::Concurrent
        Qt6::Core
        Qt6::Gui
        Qt6::Network
)

qt_add_executable(northstar-shell
    main.cpp
    layershellsurface.cpp
    shellfocus.cpp
)

qt_add_qml_module(northstar-shell
    URI Northstar.Shell
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES
        DesktopBackground.qml
        ShellWindow.qml
        DockWindow.qml
        ApplicationOverview.qml
        QuickSettings.qml
        NotificationCenterWindow.qml
        QuickLookWindow.qml
        SearchOverlay.qml
        WindowDragController.qml
        SoftwareCenterWindow.qml
        FileBrowserWindow.qml
        SystemMenu.qml
        SettingsEntryControl.qml
        SettingsWindow.qml
        WallpaperPicker.qml
)

find_package(LayerShellQt 6.6 CONFIG REQUIRED)

target_link_libraries(northstar-shell
    PRIVATE
        northstar-shell-core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        LayerShellQt::Interface
        northstar-ui
)

qt_add_executable(northstar-shell-command
    northstar-shell-command.cpp
)

target_link_libraries(northstar-shell-command
    PRIVATE
        northstar-shell-core
        Qt6::Core
        Qt6::Network
)

install(TARGETS northstar-shell northstar-shell-command
    RUNTIME DESTINATION bin
)

install(PROGRAMS ${PROJECT_SOURCE_DIR}/src/power/northstar-power
    ${PROJECT_SOURCE_DIR}/src/power/northstar-clock
    ${PROJECT_SOURCE_DIR}/src/power/northstar-radio
    DESTINATION bin
)
