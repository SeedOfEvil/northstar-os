find_package(Qt6 REQUIRED COMPONENTS QuickTest)

qt_add_executable(test-qml-surfaces
    test-qml-surfaces.cpp
)

target_link_libraries(test-qml-surfaces
    PRIVATE
        northstar-ui
        northstar-uiplugin
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickTest
)

# The tests import Northstar.Ui, whose module lives beside the northstar-ui
# build output rather than on the default import path.
add_test(NAME northstar-qml-surfaces
    COMMAND test-qml-surfaces
        -input ${CMAKE_CURRENT_SOURCE_DIR}
        -import $<TARGET_PROPERTY:northstar-ui,BINARY_DIR>
)

set_tests_properties(northstar-qml-surfaces PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_XHR_ALLOW_FILE_READ=0"
)
