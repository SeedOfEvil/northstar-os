qt_add_executable(test-shellstate
    test-shellstate.cpp
)

target_link_libraries(test-shellstate
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-shellstate COMMAND test-shellstate)

qt_add_executable(test-shortcutcatalog
    test-shortcutcatalog.cpp
)

target_link_libraries(test-shortcutcatalog
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-shortcutcatalog COMMAND test-shortcutcatalog)

qt_add_executable(test-sessioncontroller
    test-sessioncontroller.cpp
)

target_link_libraries(test-sessioncontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-sessioncontroller COMMAND test-sessioncontroller)

qt_add_executable(test-powercontroller
    test-powercontroller.cpp
)

target_link_libraries(test-powercontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-powercontroller COMMAND test-powercontroller)

qt_add_executable(test-applicationcatalog
    test-applicationcatalog.cpp
)

target_link_libraries(test-applicationcatalog
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-applicationcatalog COMMAND test-applicationcatalog)

qt_add_executable(test-applicationbundlecatalog
    test-applicationbundlecatalog.cpp
)

target_link_libraries(test-applicationbundlecatalog
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-applicationbundlecatalog COMMAND test-applicationbundlecatalog)

qt_add_executable(test-filebrowsercontroller
    test-filebrowsercontroller.cpp
)

target_link_libraries(test-filebrowsercontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-filebrowsercontroller COMMAND test-filebrowsercontroller)

qt_add_executable(test-desktopitemscontroller
    test-desktopitemscontroller.cpp
)

target_link_libraries(test-desktopitemscontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-desktopitemscontroller COMMAND test-desktopitemscontroller)

qt_add_executable(test-notificationcenter
    test-notificationcenter.cpp
)

target_link_libraries(test-notificationcenter
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-notificationcenter COMMAND test-notificationcenter)

qt_add_executable(test-packagecatalog
    test-packagecatalog.cpp
)

target_link_libraries(test-packagecatalog
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-packagecatalog COMMAND test-packagecatalog)

qt_add_executable(test-packagetrustcontroller
    test-packagetrustcontroller.cpp
)

target_link_libraries(test-packagetrustcontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-packagetrustcontroller COMMAND test-packagetrustcontroller)

qt_add_executable(test-updateplancontroller
    test-updateplancontroller.cpp
)

target_link_libraries(test-updateplancontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-updateplancontroller COMMAND test-updateplancontroller)

qt_add_executable(test-volumecatalog
    test-volumecatalog.cpp
)

target_link_libraries(test-volumecatalog
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-volumecatalog COMMAND test-volumecatalog)

qt_add_executable(test-windowcontroller
    test-windowcontroller.cpp
)

target_link_libraries(test-windowcontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-windowcontroller COMMAND test-windowcontroller)

qt_add_executable(test-pinnedapplicationmodel
    test-pinnedapplicationmodel.cpp
)

target_link_libraries(test-pinnedapplicationmodel
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-pinnedapplicationmodel COMMAND test-pinnedapplicationmodel)

qt_add_executable(test-searchcontroller
    test-searchcontroller.cpp
)

target_link_libraries(test-searchcontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-searchcontroller COMMAND test-searchcontroller)

qt_add_executable(test-previewcontroller
    test-previewcontroller.cpp
)

target_link_libraries(test-previewcontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-previewcontroller COMMAND test-previewcontroller)

qt_add_executable(test-quicksettingscontroller
    test-quicksettingscontroller.cpp
)

target_link_libraries(test-quicksettingscontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-quicksettingscontroller COMMAND test-quicksettingscontroller)

qt_add_executable(test-firstbootcontroller
    test-firstbootcontroller.cpp
)

target_link_libraries(test-firstbootcontroller
    PRIVATE
        northstar-first-boot-core
        Qt6::Test
)

add_test(NAME northstar-firstbootcontroller COMMAND test-firstbootcontroller)
add_test(NAME northstar-first-boot-qml COMMAND northstar-first-boot --self-test)

qt_add_executable(test-installercontroller test-installercontroller.cpp)
target_link_libraries(test-installercontroller PRIVATE northstar-installer-core Qt6::Test)
add_test(NAME northstar-installercontroller COMMAND test-installercontroller)
add_test(NAME northstar-installer-qml COMMAND northstar-installer --self-test)

add_test(NAME northstar-welcome-gui COMMAND northstar-welcome-gui --self-test)
set_tests_properties(northstar-welcome-gui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
qt_add_executable(test-texteditorcontroller
    test-texteditorcontroller.cpp
)

target_link_libraries(test-texteditorcontroller
    PRIVATE
        northstar-text-editor-core
        Qt6::Test
)

add_test(NAME northstar-texteditorcontroller COMMAND test-texteditorcontroller)
add_test(NAME northstar-text-editor-qml COMMAND northstar-text-editor --qml-self-test)
set_tests_properties(northstar-text-editor-qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
qt_add_executable(test-desktoplayoutcontroller
    test-desktoplayoutcontroller.cpp
)

target_link_libraries(test-desktoplayoutcontroller
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-desktoplayoutcontroller COMMAND test-desktoplayoutcontroller)

set_tests_properties(
    northstar-shellstate
    northstar-shortcutcatalog
    northstar-sessioncontroller
    northstar-powercontroller
    northstar-applicationcatalog
    northstar-applicationbundlecatalog
    northstar-filebrowsercontroller
    northstar-desktopitemscontroller
    northstar-notificationcenter
    northstar-packagecatalog
    northstar-packagetrustcontroller
    northstar-updateplancontroller
    northstar-volumecatalog
    northstar-windowcontroller
    northstar-pinnedapplicationmodel
    northstar-searchcontroller
    northstar-previewcontroller
    northstar-quicksettingscontroller
    northstar-firstbootcontroller
    northstar-first-boot-qml
    northstar-installercontroller
    northstar-installer-qml
    northstar-welcome-gui
    northstar-texteditorcontroller
    northstar-text-editor-qml
    northstar-desktoplayoutcontroller
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
