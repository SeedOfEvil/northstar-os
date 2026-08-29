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

System bundles are installed by FreeBSD packages or project package integration. They are never copied manually into system directories by the shell. A package owns every system-installed file, supplies licence/notice metadata, and records its source/package/revision identity in the manifest provenance block.

Northstar also permits an unprivileged user to install a bundle into `XDG_DATA_HOME/northstar/apps`. The installer validates the manifest and the complete source tree, requires every entry to be owned by the current user, rejects symbolic links, special files, and group- or other-writable entries, enforces bounded entry and byte counts, copies into a private staging directory, validates the staged bundle again, and publishes it with an atomic rename. The initial contract rejects duplicate identifiers instead of replacing an installed bundle. Removal applies only to a validated bundle directly below the user application root and moves it to the user's freedesktop Trash. Neither operation uses administrator privileges or mutates package-owned roots.

The launcher prefers a valid `.desktop` entry when one is available, then discovers project bundles from declared application roots. The first development roots are the user-local `XDG_DATA_HOME/northstar/apps` directory followed by `/usr/local/share/northstar/apps` and `/usr/share/northstar/apps`. A bundle can expose a `.desktop` entry for compatibility with existing FreeBSD tools. The repository includes a small `NorthstarWelcome.app` sample and the editable `NorthstarTextEditor.app` installed by the development CMake target; package integration remains a later milestone.

`NorthstarTextEditor.app` demonstrates a first-party bundle receiving a file
argument from Open With and performing an atomic user-owned save. New documents
can be saved through Save As into the user's Documents folder. The editor is
intentionally bounded to UTF-8 text documents up to 8 MiB.

## Desktop application compatibility contract

Northstar presents applications through one catalog regardless of whether the
source is a FreeBSD `.desktop` entry or a validated Northstar `.app` bundle.
The same catalog is consumed by the menu, overview, Dock, desktop icons,
Files Open With, Welcome, and Software Center. A discovered entry must expose
an honest launchability state; a missing executable, invalid icon, unsafe
manifest, or unsupported document type is shown as unavailable instead of
silently failing.

The compatibility layers are intentionally narrow and composable:

1. **Native FreeBSD applications** are the primary support path. They may be
   installed by `pkg`, launched through a validated `.desktop` entry, and use
   Xwayland when they are X11 applications.
2. **Northstar-owned bundles** provide a drag-and-drop-friendly presentation
   with explicit metadata, provenance, icons, and file arguments. They are
   packages in the user experience, not a second privileged package manager.
3. **Ported or wrapped applications** may be integrated later when a FreeBSD
   port, native build, or reviewed wrapper defines the executable, resources,
   environment, and document contract. The wrapper must remain visible in
   provenance and must not evaluate untrusted metadata through a shell.

Arbitrary macOS `.app` directories, Mach-O binaries, Apple frameworks,
Swift/Objective-C runtime assumptions, and proprietary bundle metadata are not
accepted as runnable applications. “Mac-inspired” describes Northstar's
desktop interaction and presentation; it is not a claim of binary
compatibility. Any future compatibility layer must be a separately designed,
licensed, sandboxed product slice with its own security and hardware gates.

The current first-party compatibility proof is deliberately small: Welcome
provides an actionable orientation surface, Files can route a text document to
Northstar Text Editor, and the editor performs bounded UTF-8 editing with an
atomic user-owned save. The Software Center remains read-only until signed
package publication, authorization, and rollback gates are closed; it must not
pretend that a catalog entry has been installed.

## Security and compatibility

- The executable runs as the launching user unless a separately authorized service is required.
- Bundles do not receive implicit elevated privileges.
- Private libraries must not override system libraries outside the bundle's declared runtime contract.
- The format does not load macOS Mach-O binaries, Apple frameworks, or proprietary application metadata.
- Manifest-level provenance is required for discovery; cryptographic signing and repository verification remain release-distribution work.
- The development catalog rejects symlinked bundle paths, traversal/absolute manifest paths, missing required files, invalid provenance, group/other-writable files, and non-executable bundle entry points.

The accepted decision is recorded in [`docs/adr/0005-project-app-bundle-layout.md`](adr/0005-project-app-bundle-layout.md).
