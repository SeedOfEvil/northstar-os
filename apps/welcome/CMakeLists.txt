set(NORTHSTAR_WELCOME_ICON
    ${PROJECT_SOURCE_DIR}/apps/samples/NorthstarWelcome.app/Contents/Resources/northstar-welcome.svg)
set_source_files_properties(${NORTHSTAR_WELCOME_ICON}
    PROPERTIES QT_RESOURCE_ALIAS "northstar-welcome.svg")

qt_add_executable(northstar-welcome-gui
    main.cpp
)

qt_add_qml_module(northstar-welcome-gui
    URI Northstar.Welcome
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES
        WelcomeWindow.qml
    RESOURCES
        ${NORTHSTAR_WELCOME_ICON}
)

target_link_libraries(northstar-welcome-gui
    PRIVATE
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        northstar-ui
)

install(TARGETS northstar-welcome-gui
    RUNTIME DESTINATION share/northstar/apps/NorthstarWelcome.app/Contents/Executable
)
