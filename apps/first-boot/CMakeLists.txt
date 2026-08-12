add_library(northstar-first-boot-core STATIC
    firstbootcontroller.cpp
)

target_include_directories(northstar-first-boot-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(northstar-first-boot-core
    PUBLIC
        Qt6::Core
)

set(NORTHSTAR_FIRST_BOOT_LOGO ${PROJECT_SOURCE_DIR}/assets/branding/northstar-logo.png)
set_source_files_properties(${NORTHSTAR_FIRST_BOOT_LOGO}
    PROPERTIES QT_RESOURCE_ALIAS "northstar-logo.png")

qt_add_executable(northstar-first-boot
    main.cpp
)

qt_add_qml_module(northstar-first-boot
    URI Northstar.FirstBoot
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES
        FirstBootWindow.qml
    RESOURCES
        ${NORTHSTAR_FIRST_BOOT_LOGO}
)

target_link_libraries(northstar-first-boot
    PRIVATE
        northstar-first-boot-core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        northstar-ui
)

install(TARGETS northstar-first-boot
    RUNTIME DESTINATION libexec
)

install(PROGRAMS northstar-first-boot-provision
    DESTINATION libexec
)

install(PROGRAMS northstar-first-boot-session
    DESTINATION libexec
)

install(FILES northstar-first-boot.desktop
    DESTINATION share/xsessions
)
