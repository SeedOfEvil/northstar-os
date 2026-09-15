# USB safe removal

Files labels the existing protected, non-forced unmount action **Eject**.
It does not power off hardware or eject every partition automatically. The helper
reports "Safe to unplug" only after successful unmount and a fresh native mount
inventory with no mounts for that disk. Remaining partitions or unresolved device
aliases produce a conservative "do not unplug yet" result. Protected/helper
partitions are never modified. Other applications must be closed if a drive is busy.

Eject is disabled while Files is copying. Visible Files polls storage every five
seconds; a successful inventory showing an unavailable previously mounted volume
returns its current view Home, clears its clipboard source and requests cancellation
of any active import. Inventory errors are not treated as removals. Cooperative
cancellation cannot interrupt a kernel-blocked read immediately. Completed copies
remain completed; incomplete imports are not published and staging is cleaned up.

Physical acceptance pending: mount and browse, Eject, confirm the safe-removal
message and Home navigation, then unplug/reconnect. Check a busy volume produces
an actionable error without forcing unmount. Do not unplug during real-data copying
to test failure; unit tests exercise disappearance and staging cleanup safely.
