add_library(northstar-wifi-core STATIC wificontroller.cpp)
target_include_directories(northstar-wifi-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(northstar-wifi-core PUBLIC Qt6::Core)

find_package(OpenSSL REQUIRED COMPONENTS Crypto)
add_executable(northstar-wifi-derive wifi-psk-derive.cpp)
target_link_libraries(northstar-wifi-derive PRIVATE OpenSSL::Crypto)

qt_add_executable(northstar-wifi main.cpp)
qt_add_qml_module(northstar-wifi
    URI Northstar.Wifi
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES WifiWindow.qml
)
target_link_libraries(northstar-wifi PRIVATE
    northstar-wifi-core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 northstar-ui)

install(TARGETS northstar-wifi RUNTIME DESTINATION bin)
install(TARGETS northstar-wifi-derive RUNTIME DESTINATION libexec)
install(PROGRAMS northstar-wifi-configure DESTINATION libexec)
