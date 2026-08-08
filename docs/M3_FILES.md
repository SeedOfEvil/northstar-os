# M3 Files slice

The first M3 utility is a Northstar-owned Files window. It is intentionally
small and safe for the development desktop:

- The initial location is the user's home folder.
- Navigation cannot leave that home-folder boundary, including through
  symlinks.
- Folders open in place; files are handed to the platform's default opener.
- The current folder can be refreshed, moved up, or reset to Home.
- The window is movable and resizable and is reachable from the top system menu
  and the bottom dock.

The controller is covered by a native Qt test for ordering, navigation, file
opening, and path-boundary rejection. It does not yet provide trash handling,
volume discovery, drag-and-drop, search, or `.app` association behavior; those
remain later M3 slices.

## VM validation

From the FreeBSD development checkout:

```sh
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

After the user-local shell is restarted, open **Open Files** from the system
menu or **Files** from the dock. Confirm that the home folder lists entries,
that a folder can be opened and navigated back out of, and that a regular file
opens through its default association.
