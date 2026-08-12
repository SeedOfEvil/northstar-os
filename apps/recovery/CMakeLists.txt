add_library(northstar-recovery-core STATIC
    bootenvironmentcontroller.cpp
)

target_include_directories(northstar-recovery-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(northstar-recovery-core PUBLIC Qt6::Core)

set(NORTHSTAR_RECOVERY_ICON ${PROJECT_SOURCE_DIR}/assets/icons/generated/northstar-power.png)
set_source_files_properties(${NORTHSTAR_RECOVERY_ICON}
    PROPERTIES QT_RESOURCE_ALIAS "northstar-recovery.png")

qt_add_executable(northstar-recovery main.cpp)
qt_add_qml_module(northstar-recovery
    URI Northstar.Recovery
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES RecoveryWindow.qml
    RESOURCES ${NORTHSTAR_RECOVERY_ICON}
)

target_link_libraries(northstar-recovery PRIVATE
    northstar-recovery-core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 northstar-ui)

install(TARGETS northstar-recovery
    RUNTIME DESTINATION share/northstar/apps/NorthstarRecovery.app/Contents/Executable)
install(PROGRAMS northstar-boot-environment DESTINATION libexec)
install(FILES ${PROJECT_SOURCE_DIR}/apps/samples/NorthstarRecovery.app/Contents/Info.plist
    DESTINATION share/northstar/apps/NorthstarRecovery.app/Contents)
install(FILES ${NORTHSTAR_RECOVERY_ICON}
    DESTINATION share/northstar/apps/NorthstarRecovery.app/Contents/Resources
    RENAME northstar-recovery.png)
