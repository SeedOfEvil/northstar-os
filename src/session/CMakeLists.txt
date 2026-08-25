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

install(FILES
    ${PROJECT_SOURCE_DIR}/assets/wallpapers/aurora-glass.png
    DESTINATION share/northstar/wallpapers
)

install(FILES
    ${PROJECT_SOURCE_DIR}/config/wayfire-native.ini
    ${PROJECT_SOURCE_DIR}/config/wayfire-nested.ini
    DESTINATION share/northstar/session
)

install(FILES
    ${PROJECT_SOURCE_DIR}/assets/icons/northstar-icons.png
    ${PROJECT_SOURCE_DIR}/assets/icons/northstar-icons-aurora.png
    ${PROJECT_SOURCE_DIR}/assets/icons/northstar-system-icons-aurora.png
    ${PROJECT_SOURCE_DIR}/assets/icons/northstar-activity-aurora.png
    ${PROJECT_SOURCE_DIR}/assets/icons/northstar-system-icons-aurora-light.png
    ${PROJECT_SOURCE_DIR}/assets/icons/northstar-activity-aurora-light.png
    ${PROJECT_SOURCE_DIR}/assets/icons/generated/northstar-welcome.png
    ${PROJECT_SOURCE_DIR}/assets/icons/generated/northstar-software.png
    ${PROJECT_SOURCE_DIR}/assets/icons/generated/northstar-notifications.png
    ${PROJECT_SOURCE_DIR}/assets/icons/generated/northstar-power.png
    DESTINATION share/northstar/icons
)
