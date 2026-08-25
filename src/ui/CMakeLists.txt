qt_add_library(northstar-ui STATIC
    northstarappearance.cpp
    northstarui.cpp
)

target_include_directories(northstar-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/assets/icons/northstar-system-icons-aurora.png
    PROPERTIES QT_RESOURCE_ALIAS "northstar-system-icons-aurora.png"
)
set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/assets/icons/northstar-activity-aurora.png
    PROPERTIES QT_RESOURCE_ALIAS "northstar-activity-aurora.png"
)

qt_add_qml_module(northstar-ui
    URI Northstar.Ui
    VERSION 1.0
    RESOURCE_PREFIX "/"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Northstar/Ui"
    NO_PLUGIN
    QML_FILES
        AuroraActivityIcon.qml
        AuroraButton.qml
        AuroraComboBox.qml
        AuroraSlider.qml
        LunarPalette.qml
        NativeWindowMoveHandler.qml
        NativeWindowResizeHandler.qml
        NorthstarIcon.qml
        NorthstarSystemIcon.qml
        NorthstarWindowControls.qml
        NorthstarWindowFrame.qml
        NorthstarWindowTitleBar.qml
    RESOURCES
        ${PROJECT_SOURCE_DIR}/assets/icons/northstar-system-icons-aurora.png
        ${PROJECT_SOURCE_DIR}/assets/icons/northstar-activity-aurora.png
)

target_link_libraries(northstar-ui
    PUBLIC
        Qt6::Gui
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
