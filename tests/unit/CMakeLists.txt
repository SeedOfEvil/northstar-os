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
