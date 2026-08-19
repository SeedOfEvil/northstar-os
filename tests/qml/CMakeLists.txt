find_package(Qt6 REQUIRED COMPONENTS QuickTest)

qt_add_executable(test-qml-surfaces
    test-qml-surfaces.cpp
)

target_link_libraries(test-qml-surfaces
    PRIVATE
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickTest
)

# The tests read their QML from the source tree, so they exercise the same
# files the shell module is built from rather than a copy.
add_test(NAME northstar-qml-surfaces
    COMMAND test-qml-surfaces -input ${CMAKE_CURRENT_SOURCE_DIR}
)

set_tests_properties(northstar-qml-surfaces PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_XHR_ALLOW_FILE_READ=0"
)
