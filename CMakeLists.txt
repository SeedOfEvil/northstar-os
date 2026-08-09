cmake_minimum_required(VERSION 3.28)

project(Northstar VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Test)

include(CTest)

add_subdirectory(apps/welcome)
add_subdirectory(apps/text-editor)
add_subdirectory(src/session)
add_subdirectory(src/launcher)
add_subdirectory(src/shell)
add_subdirectory(src/update)

if(BUILD_TESTING)
    add_subdirectory(tests/unit)
endif()
