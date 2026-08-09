add_executable(northstar-update-broker
    northstar-update-broker.cpp
)

target_link_libraries(northstar-update-broker
    PRIVATE
        northstar-shell-core
        Qt6::Core
)

install(TARGETS northstar-update-broker
    RUNTIME DESTINATION libexec
)
