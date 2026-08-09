# M3 desktop surface

The Desktop Icons v1 slice turns the branded background into a safe, live
desktop surface. Items are read from the user's `~/Desktop` folder and are
never resolved outside the Northstar home boundary.

## Current contract

- Files and folders appear as Northstar icon tiles over the logo wallpaper.
- Desktop changes are observed through a debounced filesystem watcher.
- A file created by Files appears without restarting the shell.
- Single click selects an item; double-click opens folders or enters the Files
  Open With flow for files.
- The context menu provides Open, Open With, Rename, Move to Trash, and
  Properties.
- Trash operations use the existing FreeDesktop-compatible user Trash and can
  be restored from Files.
- Missing `~/Desktop` is an empty, non-error state; creating the folder causes
  the surface to become available on the next watcher refresh.
- `.desktop` and `.app` items are identified as application-like entries. Their
  verified launch behavior belongs to the application-discovery slice; the
  current surface routes them through the same safe Files/Open With boundary.

## Acceptance evidence

Native coverage is in `tests/unit/test-desktopitemscontroller.cpp` and covers
stable metadata, safe open requests, Desktop-folder creation, and live
create/delete refreshes. VM acceptance must additionally confirm that a file
created in Files appears on the running desktop, folders open in Files, files
open through Open With, rename works, and Move to Trash followed by restore
removes and returns the desktop tile.

The current Proxmox VM remains a nested X11/pixman validation lane. This slice
does not claim direct DRM/KMS or GPU evidence.
