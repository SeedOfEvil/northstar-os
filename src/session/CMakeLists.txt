install(PROGRAMS northstar-session
    DESTINATION bin
)

install(PROGRAMS northstar-session-x11
    DESTINATION bin
)

install(FILES
    ${PROJECT_SOURCE_DIR}/config/session/northstar.desktop
    DESTINATION share/wayland-sessions
)

install(FILES
    ${PROJECT_SOURCE_DIR}/config/session/northstar-proxmox.desktop
    DESTINATION share/xsessions
)

install(FILES
    ${PROJECT_SOURCE_DIR}/config/sddm/northstar/metadata.desktop
    ${PROJECT_SOURCE_DIR}/config/sddm/northstar/theme.conf
    ${PROJECT_SOURCE_DIR}/config/sddm/northstar/Main.qml
    DESTINATION share/sddm/themes/northstar
)

install(FILES
    ${PROJECT_SOURCE_DIR}/config/sddm/northstar/assets/northstar-logo.png
    DESTINATION share/sddm/themes/northstar/assets
)

install(FILES
    ${PROJECT_SOURCE_DIR}/assets/branding/northstar-logo.png
    DESTINATION share/northstar/branding
)
