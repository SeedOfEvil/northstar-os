qt_add_executable(test-shellstate
    test-shellstate.cpp
)

target_link_libraries(test-shellstate
    PRIVATE
        northstar-shell-core
        Qt6::Test
)

add_test(NAME northstar-shellstate COMMAND test-shellstate)

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
