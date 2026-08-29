# ADR 0005: Project application bundle layout

Status: Accepted, amended for user-scoped installation

## Context

Northstar wants a drag-and-drop-friendly application presentation without confusing that presentation with the system package manager or promising macOS compatibility. A bundle format needs a stable executable, resource, manifest, and library layout.

## Decision

Use a project-defined directory ending in `.app` with this layout:

```text
Example.app/Contents/
  Info.plist
  Executable/Example
  Resources/
  Libraries/
```

Use `Contents/Executable` rather than `Contents/MacOS` to keep the namespace project-owned. The launcher validates the manifest, paths, permissions, and package/source provenance before execution. Standard `.desktop` applications remain first-class and bundles do not replace FreeBSD packages.

System-wide bundles remain owned and updated by FreeBSD packages. Northstar may also install a validated bundle without elevation into the current user's `XDG_DATA_HOME/northstar/apps` directory. That operation uses a private staging directory, validates the complete tree before and after copying, and publishes it with an atomic rename. The first user-install contract rejects replacement or in-place updates. It removes only bundles found in the user application root and moves them to the user's freedesktop Trash.

## Consequences

The format is visually approachable and can carry project-owned resources without claiming compatibility with proprietary bundle internals. The launcher now defines and validates a source/package/revision provenance record for development bundles. User installation is deliberately not a privileged package manager and cannot alter package-owned system applications. Packaging tools must still define cryptographic signing, repository verification, library isolation, localization, updates, and file associations before release use.

## Alternatives considered

- Reusing `Contents/MacOS`: rejected because it would unnecessarily imitate a proprietary layout.
- Supporting only bundles: rejected because it would break existing FreeBSD applications.
- Deferring all application presentation work: rejected because the format boundary is useful to define before launcher implementation.

## Validation

M3 must install a sample bundle through the development install path, validate its manifest and provenance, launch it as an unprivileged user, expose its icon/resources, and reject traversal paths or unowned executables. M4 adds package/repository and signing evidence.
