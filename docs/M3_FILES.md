# M3 Files slice

The first M3 utility is a Northstar-owned Files window. It is intentionally
small and safe for the development desktop:

- The initial location is the user's home folder.
- Navigation cannot leave that home-folder boundary, including through
  symlinks.
- Folders open in place; files are handed to the platform's default opener.
- The current folder can be refreshed, moved up, or reset to Home.
- The content area defaults to a tile-style explorer view and can be switched
  to a compact list view; folders and files open on double-click in either
  mode.
- Empty files and folders can be created, and entries can be renamed from the
  current folder.
- A selected entry has an explicit Open action; files are handed to the
  platform's default association while folders open in place.
- Entries can be moved to the per-user FreeDesktop Trash with `.trashinfo`
  metadata; operations stay inside the Northstar home-folder boundary.
- Trash is a first-class Files location with restore and confirmed empty
  actions. The selected-item action is labeled Delete to distinguish it from
  the Trash location, while still using the recoverable Trash path.
- The window is movable and resizable and is reachable from the top system menu
  and the bottom dock.

The controller is covered by a native Qt test for ordering, navigation, file
opening, path-boundary rejection, creation, renaming, Trash metadata, restore,
and empty-Trash behavior. It does not yet provide volume discovery,
drag-and-drop, search, or `.app` association behavior; those remain later M3
slices.

## VM validation

From the FreeBSD development checkout:

```sh
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

After the user-local shell is restarted, open **Open Files** from the system
menu or **Files** from the dock. Confirm that the home folder lists entries,
that a folder can be opened and navigated back out of, that a new file and
folder can be created, that a selected entry can be opened or renamed, and
that **Delete** asks for confirmation before moving the entry to Trash. Open
Trash, restore an item, and verify that **Empty Trash** asks for confirmation
before permanently removing its contents. A regular file should still open
through its default association; use **New File** to create an empty
`association-test.txt` when testing that path.
