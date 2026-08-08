# M3 Files slice

The first M3 utility is a Northstar-owned Files window. It is intentionally
small and safe for the development desktop:

- The initial location is the user's home folder.
- Navigation cannot leave that home-folder boundary, including through
  symlinks.
- Folders open in place; files are handed to the platform's default opener.
- The current folder can be refreshed, moved up, or reset to Home.
- Folders can be created and entries can be renamed from the current folder.
- Entries can be moved to the per-user FreeDesktop Trash with `.trashinfo`
  metadata; operations stay inside the Northstar home-folder boundary.
- The window is movable and resizable and is reachable from the top system menu
  and the bottom dock.

The controller is covered by a native Qt test for ordering, navigation, file
opening, path-boundary rejection, creation, renaming, and Trash metadata. It
does not yet provide volume discovery, drag-and-drop, search, or `.app`
association behavior; those remain later M3 slices.

## VM validation

From the FreeBSD development checkout:

```sh
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

After the user-local shell is restarted, open **Open Files** from the system
menu or **Files** from the dock. Confirm that the home folder lists entries,
that a folder can be opened and navigated back out of, that a new folder can be
created, that a selected entry can be renamed, and that Trash asks for
confirmation before moving the entry. A regular file should still open through
its default association.
