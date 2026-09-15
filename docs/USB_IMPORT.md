# USB to Home imports

Branch: `codex/m7-usb-import`. Physical acceptance pending.

Files already permits copying from mounted locations and restricts writes to
Home. The Copy button now becomes Copy to Home when browsing such a location.
It selects the source, navigates to Home and uses the normal paste/conflict flow.
Existing Copy/Paste can still target a chosen Home subfolder. No USB write mount,
move-from-USB, overwriting conflict mode or new privilege is added.

External copies use bounded tree planning (100,000 entries, depth 64), reject
symlinks/special files and stream regular files in 1 MiB chunks. Source size and
modification time are checked. A private staging directory under the destination
holds partial data; only a complete copy is renamed into place, without replacing
existing content. Normal cancellation or failure removes staging. Unexpected
process termination may leave a hidden staging directory for later recovery.

The toolbar reports copied/total MiB and byte-based progress and offers Cancel
copy. Cancellation is cooperative between reads, not a guarantee that a blocked
kernel read can be interrupted instantly. Home copies/moves retain their existing
behavior. An import cancelled after publication has completed is a completed copy;
Undo moves that copy to Trash.

Tests cover imported file contents, directory imports, Keep Both, pre-cancellation,
unsafe links, and preservation of existing destination data on failed publication.
Real mounted-volume discovery is also covered without injected test roots. A
physical logging-folder attempt exposed that the original source check accepted
only injected roots; the active volume is now revalidated against mounted volumes.
The native regression suite passes, but the corrected GUI copy still needs
physical acceptance.
Physical acceptance: import a disposable USB file, open/check its contents in
Home, verify the original remains on the read-only USB, test Keep Both and cancel
a sufficiently large copy. Do not unplug a mounted device to simulate failure.
