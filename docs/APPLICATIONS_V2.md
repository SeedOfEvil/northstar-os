# Applications v2 delivery plan

Applications v2 expands Northstar's application experience without pretending that a FreeBSD system can execute macOS software or creating a second privileged package manager.

## Delivery sequence

1. **User bundle foundation** — inspect, install, and remove validated Northstar `.app` bundles without elevation. System applications remain package-owned.
2. **Graphical workflow** — expose install, conflict, provenance, and Trash-backed removal flows in Files and Applications with clear confirmation and failure states.
3. **Developer packaging kit** — provide a reproducible packager, schema validation, licence metadata, and test fixtures for native FreeBSD applications.
4. **Web application bundles** — package reviewed PWAs with explicit origins, permissions, storage boundaries, and offline behavior.
5. **Portability reports** — tell users whether an application is native, wrapped, web-based, unavailable, or needs a FreeBSD port; do not imply unsupported compatibility.
6. **Compatibility research** — evaluate Mach-O formats and legal/runtime boundaries in a separate ADR and experimental branch. No foreign binary runs through the trusted Northstar launcher until that work has its own threat model and acceptance gates.

## Current implementation

- The user bundle foundation provides bounded, atomic, no-root install and Trash-backed removal.
- Files recognizes `.app` directories, displays validated identity and provenance, and asks for confirmation before installation.
- Applications displays bundle provenance and offers removal only for bundles installed in the current user's application root.
- A user bundle cannot replace or shadow an installed package-owned bundle with the same identifier.
- The [developer packaging kit](APP_PACKAGING_KIT.md) assembles a validated,
  unsigned bundle from a versioned recipe and finished native build inputs,
  including licence text. Its graphical sample passed focused Intel acceptance.

The [web-bundle browser-launch slice](WEB_APP_BUNDLES.md) adds named website
shortcuts with explicit shared-browser and online requirements. It does not
yet close the broader reviewed-PWA/profile/offline milestone.

## Foundation acceptance

- A valid, current-user-owned `.app` installs beneath `XDG_DATA_HOME/northstar/apps` without root.
- The source is unchanged and publication is atomic.
- Traversal, symlinks, special files, unsafe permissions, invalid provenance, excessive size, and duplicate identifiers are rejected.
- Only validated user-installed bundles can be removed, and removal moves the bundle to Trash.
- Package-owned applications and system application roots remain untouched.
- Focused tests pass on native FreeBSD before the change is considered merge-ready.

## Explicit non-goals for this slice

- no arbitrary macOS or Mach-O execution;
- no root or PolicyKit broker;
- no silent replacement, update, or downgrade;
- no system package removal;
- no claim that a manifest provenance record is a cryptographic signature.
