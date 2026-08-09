# M3 file associations

Northstar now supports user-scoped default applications for regular files.
The first contract is intentionally extension-based so it is predictable and
does not override FreeBSD or other desktop environment settings.

When a file has no saved Northstar default, **Open** opens the project-owned
Open With chooser. The chooser lists the current `.desktop` and project `.app`
catalog, prioritizes applications that declare support for the file's MIME type
or extension, accepts a one-time launch, and can remember the selected
application for that extension. A later **Open** launches the saved application
directly.
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

Project `.app` bundles may declare a `DocumentExtensions` array in `Info.plist`.
XDG `.desktop` entries use their standard `MimeType` list. The chooser keeps a
**Show All** escape hatch when no compatible application is discovered, so a
user can still make an intentional one-time choice without treating every app
as an editor by default.

## Evidence

The native `ApplicationCatalogTest` covers invalid application IDs,
case-normalized extension lookup, persistence across launcher instances, and
clearing a saved association. The bundle catalog and launcher tests also cover
declared document extensions and compatible-app filtering. The Files VM check
should create an
`association-test.txt`, remember an application, verify direct reopening,
then use **Open With...** and **Forget Default** to restore chooser behavior.

Files with a conventional extension continue to use a lowercase extension
key. Extensionless files now use their detected Qt MIME type as a fallback,
so the chooser can remember a type-level default without changing the system
MIME database. The dialog labels this case as “files of this type”; unknown
types still remain user-scoped and never modify package-owned associations.
