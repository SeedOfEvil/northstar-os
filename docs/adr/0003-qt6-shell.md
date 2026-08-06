# ADR 0003: Qt 6, C++20, and QML shell

Status: Accepted

## Context

Northstar needs a cohesive toolkit for the shell, project-owned applications, accessibility, settings, and visual design tokens. The first implementation should remain within an established FreeBSD package ecosystem and should support Wayland, D-Bus, and declarative UI.

## Decision

Build the shell and first-party applications with Qt 6, C++20, and QML. Use CMake and Ninja for builds, QtDBus for D-Bus integration, and the FreeBSD-packaged layer-shell Qt integration where available. Keep compositor-specific code behind a project-owned platform interface.

## Consequences

The project shares a toolkit across the desktop and can iterate quickly on coherent UI. Qt package versions and licence terms must be recorded. Third-party applications remain free to use other toolkits; Northstar does not require a universal global menu for them.

## Alternatives considered

- GTK as the primary toolkit: not selected for the initial shell because the project wants one Qt/QML implementation across shell and first-party applications.
- A custom rendering toolkit: rejected as unnecessary infrastructure.
- SwiftUI or macOS frameworks: outside the legal and technical scope.

## Validation

M0 must install the Qt 6 development package set. M1 must build a native shell, render a QML surface, and pass Qt unit tests on FreeBSD.
