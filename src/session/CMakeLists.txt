install(PROGRAMS northstar-session
    DESTINATION bin
)

install(FILES
    ${PROJECT_SOURCE_DIR}/config/session/northstar.desktop
    DESTINATION share/wayland-sessions
)
