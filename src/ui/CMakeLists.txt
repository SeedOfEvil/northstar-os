qt_add_library(northstar-ui STATIC
    northstarappearance.cpp
    northstarui.cpp
)

target_include_directories(northstar-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_qml_module(northstar-ui
    URI Northstar.Ui
    VERSION 1.0
    RESOURCE_PREFIX "/"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Northstar/Ui"
    NO_PLUGIN
    QML_FILES
        AuroraButton.qml
        AuroraComboBox.qml
        AuroraSlider.qml
        LunarPalette.qml
        NativeWindowMoveHandler.qml
        NativeWindowResizeHandler.qml
        NorthstarIcon.qml
        NorthstarWindowControls.qml
        NorthstarWindowFrame.qml
        NorthstarWindowTitleBar.qml
)

target_link_libraries(northstar-ui
    PUBLIC
        Qt6::Gui
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
