# ADR 0002: Wayland and Wayfire bootstrap

Status: Accepted

## Context

The desktop needs a modern display protocol, X11 application compatibility, and a compositor that can run early without requiring Northstar to implement a compositor. FreeBSD packages Wayland, Xwayland, seat management, and Wayfire.

## Decision

Use Wayland as the graphics protocol, Xwayland for X11 compatibility, and FreeBSD-packaged Wayfire as the initial compositor. The shell communicates through Wayland protocols and a small platform adapter. It must not depend on undocumented Wayfire internals.

## Consequences

The first shell can be delivered without a custom compositor. Wayfire remains replaceable, and compositor-specific behavior is isolated. Protocol and layer-shell availability must be tested on the supported FreeBSD package set. Proprietary NVIDIA Wayland behavior is not an alpha requirement.

## Alternatives considered

- Writing a Northstar compositor: deferred until the desktop product proves it needs one.
- X11 as the primary protocol: rejected because Wayland is the target for new shell surfaces.
- Binding directly to Wayfire internals: rejected because it creates avoidable lock-in.

## Validation

M0 must start Wayfire unprivileged, launch a native Wayland Qt application, and launch an X11 application through Xwayland. M1 must verify layer-shell behavior on every connected display.
