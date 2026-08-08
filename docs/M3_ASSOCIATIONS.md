# M3 file associations

Northstar now supports user-scoped default applications for regular files.
The first contract is intentionally extension-based so it is predictable and
does not override FreeBSD or other desktop environment settings.

When a file has no saved Northstar default, **Open** opens the project-owned
Open With chooser. The chooser lists the current `.desktop` and project `.app`
catalog, accepts a one-time launch, and can remember the selected application
for that extension. A later **Open** launches the saved application directly.
**Open With...** always remains available so the user can select another
application, and **Forget Default** removes the Northstar preference.

Preferences are stored in the user-scoped Qt configuration location as
`file-associations.ini`. The launcher validates that a saved desktop ID still
exists in the current catalog before using it; stale or unknown IDs are
ignored. Extensions are normalized to lowercase and restricted to safe
letters, numbers, hyphens, and underscores. Files without a conventional
extension do not receive a saved association in this first slice.

This store is deliberately separate from system-wide MIME defaults. It does
not edit `/usr/local/share/applications`, `mimeapps.list`, or package-owned
files, and it never requires root access.

## Evidence

The native `ApplicationCatalogTest` covers invalid application IDs,
case-normalized extension lookup, persistence across launcher instances, and
clearing a saved association. The Files VM check should create an
`association-test.txt`, remember an application, verify direct reopening,
then use **Open With...** and **Forget Default** to restore chooser behavior.
