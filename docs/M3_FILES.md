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
- The content toolbar can sort by name, type, size, or modification time and
  can reverse the order while keeping folders ahead of regular files.
- Empty files and folders can be created, and entries can be renamed from the
  current folder.
- A selected entry has an explicit Open action; files can be opened with a
  registered application or the platform's explicit default association. The
  Open With action remains available even after a default is saved.
- The Open With chooser can remember one user-scoped application per file
  extension and can forget that choice later; it never changes system-wide
  desktop associations.
- Entries can be moved to the per-user FreeDesktop Trash with `.trashinfo`
  metadata; operations stay inside the Northstar home-folder boundary.
- Trash is a first-class Files location with restore and confirmed empty
  actions. The selected-item action is labeled Delete to distinguish it from
  the Trash location, while still using the recoverable Trash path.
- The Locations bar shows Home, Trash, and mounted non-pseudo volumes
  discovered through Qt's storage API. Mounted-volume browsing is read-only;
  file creation, rename, delete, and search remain scoped to Home.
- The window is movable and resizable and is reachable from the top system menu
  and the bottom dock.
- Home-folder search recursively finds matching file and folder names, shows
  each result's relative location, excludes the Northstar Trash store, and
  caps results to keep interactive search bounded.
- Regular files can be dragged from the Files explorer onto an application
  tile in Apps; the existing launcher validates the file and passes it as the
  selected application's argument.

The controller is covered by a native Qt test for ordering, navigation, file
opening, path-boundary rejection, creation, renaming, Trash metadata, restore,
empty-Trash behavior, bounded home-folder search, and read-only mounted-volume
navigation. The launcher also tests persistence and validation of the
user-scoped extension association store. Package provenance is complete for
the current `.app` contract; the project-owned global menu remains outside
the application-level shortcut slice.

## Files v3 Finder-style surface

The Files v3 surface keeps the v2 safety and mutation contracts while giving
the window a more familiar Finder-style hierarchy:

- a persistent sidebar separates Favorites from the current location;
- Home, Desktop, Documents, Downloads, and Trash are presented as direct
  destinations when those home-scoped folders exist;
- the content canvas is wider on normal displays and collapses the sidebar on
  narrow displays so the file grid remains usable;
- Open With includes an application search field and resets its query when the
  chooser closes, while the reversible per-extension default remains explicit;
- sidebar destinations resolve through the controller's home-boundary check,
  so visual navigation cannot bypass the existing symlink and mounted-volume
  restrictions.

This is a visual and navigation layer over the v2 controller, not a claim of
system-wide Finder compatibility. Trash remains the recoverable FreeDesktop
path, mounted volumes remain read-only, and application associations remain
user-scoped.

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
empty `association-test.txt` when testing that path. Choose an application,
enable **Remember this choice**, open the file again to confirm it launches
directly, then use **Open With...** and **Forget Default** to confirm the
choice is reversible. Drag that file onto an application tile in **Apps** and
confirm the selected application receives it;
folders and Trash entries should not start a drag operation. In **Locations**,
open each mounted volume, navigate into a folder, verify the read-only label
and disabled mutation/search controls, then return to **Home**.

For the v3 surface, confirm that the sidebar highlights the active location,
that existing Desktop/Documents/Downloads folders open directly, that a
missing favorite is visibly disabled, and that the Open With search narrows the
application list without changing the saved default until the user selects an
application. Use the sort selector to switch between Name, Type, Size, and
Modified, reverse the order, and verify that folders remain ahead of files in
both directions.
