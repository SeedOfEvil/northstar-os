add_library(northstar-installer-core STATIC
    installerdiskmodel.cpp
    installercontroller.cpp
)

target_include_directories(northstar-installer-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(northstar-installer-core PUBLIC Qt6::Core)

set(NORTHSTAR_INSTALLER_LOGO ${PROJECT_SOURCE_DIR}/assets/branding/northstar-logo.png)
set_source_files_properties(${NORTHSTAR_INSTALLER_LOGO}
    PROPERTIES QT_RESOURCE_ALIAS "northstar-logo.png")

qt_add_executable(northstar-installer main.cpp)
qt_add_qml_module(northstar-installer
    URI Northstar.Installer
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES InstallerWindow.qml
    RESOURCES ${NORTHSTAR_INSTALLER_LOGO}
)

target_link_libraries(northstar-installer PRIVATE
    northstar-installer-core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 northstar-ui)

install(TARGETS northstar-installer RUNTIME DESTINATION libexec)
install(PROGRAMS northstar-installer-disks DESTINATION libexec)
install(PROGRAMS northstar-installer-engine DESTINATION libexec)
install(PROGRAMS northstar-installer-source-verify DESTINATION libexec)
