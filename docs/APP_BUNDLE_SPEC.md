# Northstar application bundle specification

## Status

Version 0.1, accepted for design and implementation. The first development bundle is now discoverable and launchable through the Northstar catalog after manifest, path, ownership, permission, and manifest-level provenance validation. This is a Northstar presentation and portability format. It is not a promise of macOS compatibility and it does not replace FreeBSD packages.

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
| `Provenance` | dictionary | Required source, package, and revision identity |
| `DesktopFile` | string, optional | Associated `.desktop` file when one exists |
| `MinimumNorthstar` | string, optional | Minimum shell/runtime contract |

`Provenance` must contain these non-empty string keys:

| Key | Meaning |
| --- | --- |
| `Source` | Human-readable source family or project that produced the bundle |
| `Package` | Package or component identity that owns the bundle |
| `Revision` | Source/package revision, release identifier, or explicit development marker |

Values are bounded, trimmed, and free of control characters. Additional provenance keys may be added without changing the initial launcher contract. This is an auditable manifest identity, not a cryptographic signature.

Unknown keys are ignored by older launchers but must not weaken validation. Paths must be relative and must not contain `..` components.

## Installation and updates

Bundles are installed by FreeBSD packages or a future project package integration. They are not copied manually into system directories by the shell. A package owns every installed file, supplies licence/notice metadata, and records its source/package/revision identity in the manifest provenance block.

The launcher prefers a valid `.desktop` entry when one is available, then discovers project bundles from declared application roots. The first development roots are the user-local `XDG_DATA_HOME/northstar/apps` directory followed by `/usr/local/share/northstar/apps` and `/usr/share/northstar/apps`. A bundle can expose a `.desktop` entry for compatibility with existing FreeBSD tools. The repository includes a small `NorthstarWelcome.app` sample and the editable `NorthstarTextEditor.app` installed by the development CMake target; package integration remains a later milestone.

`NorthstarTextEditor.app` demonstrates a first-party bundle receiving a file
argument from Open With and performing an atomic user-owned save. The editor
is intentionally bounded to UTF-8 text documents up to 8 MiB.

## Security and compatibility

- The executable runs as the launching user unless a separately authorized service is required.
- Bundles do not receive implicit elevated privileges.
- Private libraries must not override system libraries outside the bundle's declared runtime contract.
- The format does not load macOS Mach-O binaries, Apple frameworks, or proprietary application metadata.
- Manifest-level provenance is required for discovery; cryptographic signing and repository verification remain release-distribution work.
- The development catalog rejects symlinked bundle paths, traversal/absolute manifest paths, missing required files, invalid provenance, group/other-writable files, and non-executable bundle entry points.

The accepted decision is recorded in [`docs/adr/0005-project-app-bundle-layout.md`](adr/0005-project-app-bundle-layout.md).
