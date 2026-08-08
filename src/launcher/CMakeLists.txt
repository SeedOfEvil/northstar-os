add_library(northstar-launcher-core STATIC
    applicationcatalog.cpp
    applicationbundlecatalog.cpp
)

target_include_directories(northstar-launcher-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(northstar-launcher-core
    PUBLIC
        Qt6::Core
)

install(FILES
    ${PROJECT_SOURCE_DIR}/apps/samples/NorthstarWelcome.app/Contents/Info.plist
    DESTINATION share/northstar/apps/NorthstarWelcome.app/Contents
)

install(PROGRAMS
    ${PROJECT_SOURCE_DIR}/apps/samples/NorthstarWelcome.app/Contents/Executable/northstar-welcome
    DESTINATION share/northstar/apps/NorthstarWelcome.app/Contents/Executable
)

install(FILES
    ${PROJECT_SOURCE_DIR}/apps/samples/NorthstarWelcome.app/Contents/Resources/northstar-welcome.svg
    DESTINATION share/northstar/apps/NorthstarWelcome.app/Contents/Resources
)
