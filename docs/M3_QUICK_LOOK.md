# M3 Quick Look

Northstar Quick Look previews a selected item without opening it, changing its
file association, executing it, or modifying its contents. Select an item in
Files or on the desktop and press Space, or choose the visible Quick Look
action.

## Preview contract

`PreviewController` exposes explicit `empty`, `ready`, and `error` status plus
one of these preview kinds:

- `text`: valid UTF-8 text, capped at a 128 KB rendered excerpt;
- `image`: a locally decoded raster image scaled to at most 960 by 640;
- `folder`: a bounded directory scan with up to 12 displayed names;
- `metadata`: file type, size, modification time, and an unavailable reason;
- `error`: a missing, unreadable, or disallowed target.

Text files larger than 8 MB and raster images larger than 32 MB or 40 million
source pixels fall back to metadata. Folder scans stop after 500 entries.
Unsupported and oversized files are never executed or fully loaded merely to
produce a preview.

## Path boundary

Desktop previews resolve beneath canonical Home. Files previews may also use
the root of the explicitly browsed mounted volume. Canonical containment is
rechecked by the controller; `/`, arbitrary sibling paths, and missing targets
are rejected. Previewing never changes the existing Open or Open With path.

## Interaction acceptance

At 1280x800, validate that:

1. Space and the Quick Look mouse action open the same selected item from
   Files and the desktop.
2. The Quick Look panel moves, resizes, maximizes/restores, closes, and remains
   unclipped.
3. A UTF-8 text file, local raster image, folder, unsupported file, and missing
   target each present the expected explicit state.
4. Closing Quick Look leaves the Files selection, association, and contents
   unchanged.
5. Home-scoped items work while arbitrary paths remain blocked; an explicitly
   browsed mounted volume remains read-only.

Automated coverage lives in `northstar-previewcontroller` and the QML surface
contract. The scfb/pixman VM remains the interaction lane and does not close
direct DRM/KMS or GPU-animation gates.
