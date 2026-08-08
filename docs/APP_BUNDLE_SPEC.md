# Northstar application bundle specification

## Status

Version 0.1, accepted for design and implementation. The first development bundle is now discoverable and launchable through the Northstar catalog after manifest, path, ownership, and permission validation. This is a Northstar presentation and portability format. It is not a promise of macOS compatibility and it does not replace FreeBSD packages.

## Goals

- make a Northstar application discoverable and launchable as one user-facing item;
- carry an executable, icon, resources, translations, and optional private libraries;
- preserve standard FreeBSD package installation and update behavior;
- allow the launcher to support both `.desktop` applications and bundles;
- keep the layout project-owned and legally distinct from proprietary bundle formats.

## Layout

The first implementation uses `Contents/Executable`, not `Contents/MacOS`, to make the namespace explicitly Northstar-owned:

```text
Example.app/
`-- Contents/
    |-- Info.plist
    |-- Executable/
    |   `-- Example
    |-- Resources/
    |   |-- icon.svg
    |   `-- translations/
    `-- Libraries/
```

`Example.app` is a directory with a case-sensitive `.app` suffix. The launcher must never execute an arbitrary file merely because it has that suffix; it validates the manifest, executable path, ownership, permissions, and package/source provenance first.

## Manifest

`Contents/Info.plist` is a project-defined property-list document. The initial required keys are:

| Key | Type | Meaning |
| --- | --- | --- |
| `BundleIdentifier` | string | Reverse-domain-style Northstar identifier, stable across updates |
| `DisplayName` | string | Localized user-facing application name |
| `Version` | string | Application version |
| `Executable` | string | Relative path below `Contents/Executable/` |
| `Icon` | string | Relative path below `Contents/Resources/` |
| `Categories` | array | Launcher categories |
| `DesktopFile` | string, optional | Associated `.desktop` file when one exists |
| `MinimumNorthstar` | string, optional | Minimum shell/runtime contract |

Unknown keys are ignored by older launchers but must not weaken validation. Paths must be relative and must not contain `..` components.

## Installation and updates

Bundles are installed by FreeBSD packages or a future project package integration. They are not copied manually into system directories by the shell. A package owns every installed file and supplies licence/notice metadata.

The launcher prefers a valid `.desktop` entry when one is available, then discovers project bundles from declared application roots. The first development roots are the user-local `XDG_DATA_HOME/northstar/apps` directory followed by `/usr/local/share/northstar/apps` and `/usr/share/northstar/apps`. A bundle can expose a `.desktop` entry for compatibility with existing FreeBSD tools. The repository includes a small `NorthstarWelcome.app` sample installed by the development CMake target; package integration remains a later milestone.

## Security and compatibility

- The executable runs as the launching user unless a separately authorized service is required.
- Bundles do not receive implicit elevated privileges.
- Private libraries must not override system libraries outside the bundle's declared runtime contract.
- The format does not load macOS Mach-O binaries, Apple frameworks, or proprietary application metadata.
- Signing and provenance requirements will be defined before bundles are used for release distribution.
- The development catalog rejects symlinked bundle paths, traversal/absolute manifest paths, missing required files, group/other-writable files, and non-executable bundle entry points.

The accepted decision is recorded in [`docs/adr/0005-project-app-bundle-layout.md`](adr/0005-project-app-bundle-layout.md).
