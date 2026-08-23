add_library(northstar-bluetooth-core STATIC bluetoothcontroller.cpp)
target_include_directories(northstar-bluetooth-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(northstar-bluetooth-core PUBLIC Qt6::Core)

qt_add_executable(northstar-bluetooth main.cpp)
qt_add_qml_module(northstar-bluetooth
    URI Northstar.Bluetooth
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES BluetoothWindow.qml
)
target_link_libraries(northstar-bluetooth PRIVATE
    northstar-bluetooth-core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 northstar-ui)

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    add_executable(northstar-bluetooth-ssp northstar-bluetooth-ssp.cpp)
    target_link_libraries(northstar-bluetooth-ssp PRIVATE bluetooth)
    install(TARGETS northstar-bluetooth-ssp RUNTIME DESTINATION libexec)
endif()

install(TARGETS northstar-bluetooth RUNTIME DESTINATION bin)
install(PROGRAMS
    northstar-bluetooth-scan
    northstar-bluetooth-configure
    northstar-bluetooth-receive
    DESTINATION libexec)
