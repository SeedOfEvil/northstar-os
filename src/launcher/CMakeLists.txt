add_library(northstar-launcher-core STATIC
    applicationcatalog.cpp
)

target_include_directories(northstar-launcher-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(northstar-launcher-core
    PUBLIC
        Qt6::Core
)
