# Files removable sidebar

Feature branch: `codex/m7-usb-sidebar`. Physical acceptance pending.

Files lists eligible USB data partitions in a scrollable sidebar. Clicking an
unmounted entry uses the existing authorized read-only mount-and-browse flow;
clicking a verified mounted entry opens that location. Unmount uses the existing
non-forced helper and displays its completion or busy/error response. It does
not claim whole-drive eject or that every partition is safe to unplug.

While Files is visible, a five-second timer refreshes metadata. Refresh pauses
while the Devices dialog or its authorization operation is active. Scans cannot
overlap. Startup discovery remains enabled. No automatic mount, writes, repair,
formatting or broadening of eligible devices is added. The boot helper is not an
actionable sidebar entry. Existing write boundaries remain unchanged.

Acceptance: attached NTFS USB appears without manual refresh; clicking opens
the mounted volume; unplug/replug is reflected within a scan interval; explicit
unmount gives accurate feedback; shorter Files windows keep controls scrollable;
cancelled authorization does not open an unmounted path. No installer rebuild is
required for this shell-only UI follow-up.
