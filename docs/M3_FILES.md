# M3 Files slice

The first M3 utility is a Northstar-owned Files window. It is intentionally
small and safe for the development desktop:

- The initial location is the user's home folder.
- Navigation cannot leave that home-folder boundary, including through
  symlinks.
- Folders open in place; files are offered to a Northstar Open With chooser.
- The current folder can be refreshed, moved up, or reset to Home.
- The content area defaults to a tile-style explorer view and can be switched
  to a compact list view; folders and files open on double-click in either
  mode.
- Empty files and folders can be created, and entries can be renamed from the
  current folder.
- A selected entry has an explicit Open action; files can be opened with a
  registered application or the platform's explicit default association.
- Entries can be moved to the per-user FreeDesktop Trash with `.trashinfo`
  metadata; operations stay inside the Northstar home-folder boundary.
- Trash is a first-class Files location with restore and confirmed empty
  actions. The selected-item action is labeled Delete to distinguish it from
  the Trash location, while still using the recoverable Trash path.
- The window is movable and resizable and is reachable from the top system menu
  and the bottom dock.
- Home-folder search recursively finds matching file and folder names, shows
  each result's relative location, excludes the Northstar Trash store, and
  caps results to keep interactive search bounded.

The controller is covered by a native Qt test for ordering, navigation, file
opening, path-boundary rejection, creation, renaming, Trash metadata, restore,
empty-Trash behavior, and bounded home-folder search. The shell presents an
explicit Open With chooser for registered desktop applications. Volume
discovery, drag-and-drop, and `.app` association behavior remain later M3
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
before permanently removing its contents. Use the **Search the Northstar home
folder** field to find a nested item, verify its relative location is shown,
and clear the query before returning to normal navigation. A regular file
should open the Northstar Open With chooser; use **New File** to create an
empty `association-test.txt` when testing that path.
