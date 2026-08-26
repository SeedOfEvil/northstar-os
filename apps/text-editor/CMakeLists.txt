add_library(northstar-text-editor-core STATIC
    recentfilesstore.cpp
    texteditorcontroller.cpp
)

set(NORTHSTAR_TEXT_EDITOR_ICON
    ${PROJECT_SOURCE_DIR}/apps/text-editor/resources/northstar-text-editor.svg)
set_source_files_properties(${NORTHSTAR_TEXT_EDITOR_ICON}
    PROPERTIES QT_RESOURCE_ALIAS "northstar-text-editor.svg")

target_include_directories(northstar-text-editor-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(northstar-text-editor-core
    PUBLIC
        Qt6::Core
)

qt_add_executable(northstar-text-editor
    main.cpp
)

qt_add_qml_module(northstar-text-editor
    URI Northstar.TextEditor
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES
        TextEditorWindow.qml
    RESOURCES
        ${NORTHSTAR_TEXT_EDITOR_ICON}
)

target_link_libraries(northstar-text-editor
    PRIVATE
        northstar-text-editor-core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        northstar-ui
)

set(NORTHSTAR_TEXT_EDITOR_MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/NorthstarTextEditor-Info.plist)
configure_file(
    ${PROJECT_SOURCE_DIR}/apps/samples/NorthstarTextEditor.app/Contents/Info.plist.in
    ${NORTHSTAR_TEXT_EDITOR_MANIFEST}
    @ONLY
)

install(TARGETS northstar-text-editor
    RUNTIME DESTINATION share/northstar/apps/NorthstarTextEditor.app/Contents/Executable
)

install(FILES
    ${NORTHSTAR_TEXT_EDITOR_MANIFEST}
    DESTINATION share/northstar/apps/NorthstarTextEditor.app/Contents
    RENAME Info.plist
)

install(FILES
    ${PROJECT_SOURCE_DIR}/apps/text-editor/resources/northstar-text-editor.svg
    DESTINATION share/northstar/apps/NorthstarTextEditor.app/Contents/Resources
)
