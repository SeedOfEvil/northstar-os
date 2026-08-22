cmake_minimum_required(VERSION 3.28)

set(NORTHSTAR_PACKAGE_VERSION "0.1.0" CACHE STRING "Northstar package version")

project(Northstar
    VERSION ${NORTHSTAR_PACKAGE_VERSION}
    DESCRIPTION "Northstar desktop environment for FreeBSD"
    HOMEPAGE_URL "https://github.com/SeedOfEvil/northstar-os"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Concurrent Core Gui Network Qml Quick QuickControls2 Test)

include(CTest)

add_subdirectory(src/ui)
add_subdirectory(apps/installer)
add_subdirectory(apps/recovery)
add_subdirectory(apps/first-boot)
add_subdirectory(apps/wifi)
add_subdirectory(apps/welcome)
add_subdirectory(apps/text-editor)
add_subdirectory(src/session)
add_subdirectory(src/launcher)
add_subdirectory(src/shell)
add_subdirectory(src/update)

if(BUILD_TESTING)
    add_subdirectory(tests/unit)
    add_subdirectory(tests/qml)
endif()

set(CPACK_GENERATOR "FREEBSD")
set(CPACK_PACKAGE_NAME "northstar")
set(CPACK_PACKAGE_VENDOR "Northstar contributors")
set(CPACK_PACKAGE_CONTACT "Northstar contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Northstar desktop environment for FreeBSD")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/SeedOfEvil/northstar-os")
set(CPACK_PACKAGE_FILE_NAME "northstar-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_FREEBSD_PACKAGE_NAME "northstar")
set(CPACK_FREEBSD_PACKAGE_COMMENT "Northstar desktop environment")
set(CPACK_FREEBSD_PACKAGE_DESCRIPTION "Northstar provides a cohesive desktop shell, session, and first-party applications for FreeBSD.")
set(CPACK_FREEBSD_PACKAGE_WWW "https://github.com/SeedOfEvil/northstar-os")
set(CPACK_FREEBSD_PACKAGE_LICENSE "BSD2CLAUSE")
set(CPACK_FREEBSD_PACKAGE_MAINTAINER "northstar@localhost.invalid")
set(CPACK_FREEBSD_PACKAGE_ORIGIN "x11/northstar")
set(CPACK_FREEBSD_PACKAGE_CATEGORIES "x11")
set(CPACK_FREEBSD_PACKAGE_DEPS
    "devel/qt6-base;x11-toolkits/qt6-declarative;graphics/qt6-svg;graphics/qt6-wayland;x11/plasma6-layer-shell-qt;sysutils/lxqt-policykit"
)

install(FILES packaging/polkit/org.northstar.update.policy
              packaging/polkit/org.northstar.package.policy
              packaging/polkit/org.northstar.firstboot.policy
              packaging/polkit/org.northstar.wifi.policy
              packaging/polkit/org.northstar.installer.policy
              packaging/polkit/org.northstar.recovery.policy
    DESTINATION share/polkit-1/actions
)

include(CPack)
