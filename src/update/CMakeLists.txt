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

install(PROGRAMS
    northstar-update-helper
    northstar-update-transaction
    northstar-package-transaction
    ${PROJECT_SOURCE_DIR}/image/scripts/validate-image-update-rollback.sh
    DESTINATION libexec
)

install(FILES
    ${PROJECT_SOURCE_DIR}/packaging/manifests/third-party-package-source.conf
    DESTINATION etc/northstar
)
