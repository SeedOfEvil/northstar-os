# Northstar seven-hour execution runbook

This runbook expands the broad roadmap into small, observable work units for a
long unattended Northstar sprint. The time ranges are relative to the start of
the run. They are timeboxes, not permission to claim a gate without evidence.

The run should produce several user-visible improvements, a repeatable VM
validation record, and a clean GitHub history. It must continue making useful
progress when interactive noVNC validation is unavailable, while explicitly
leaving that gate open instead of guessing.

## Operating contract

Use one `codex/` branch and one draft PR for each cohesive product slice. A
micro-step is normally a task inside that slice; it is not automatically a new
branch. At every slice checkpoint:

1. Define the user-visible behavior and the boundary of the change.
2. Implement the smallest coherent increment.
3. Add or update the narrowest native/unit/QML check that proves it.
4. Run the targeted check locally and on the separate VM validation checkout.
5. Install the exact tested source into `/home/northstar/.local`.
6. Record the commit, VM path, command, result, and any limitation.
7. Commit, push, open a draft PR, and include the evidence in its body.
8. Promote and squash-merge only after the applicable manual gate passes.
9. Delete the merged feature refs only after verifying the squash merge.

Do not modify `/home/northstar/src/northstar`; it is the canonical VM checkout.
Use a separately named validation checkout. Do not claim direct DRM/KMS,
native multi-display, or Intel/AMD hardware support from the current
Proxmox/scfb lane. If a UI gate cannot be observed, mark it `OPEN` and use
headless work, documentation, tests, or a different product slice to keep
moving.

## Slice map

The seven-hour run is divided into eight checkpoints:

| Slice | Target window | Branch/PR boundary | Visible outcome |
| --- | --- | --- | --- |
| S0 | 00:00-00:30 | baseline only | reproducible starting point and clean validation lane |
| S1 | 00:30-01:20 | Desktop surface | live Desktop projection and reliable icon actions |
| S2 | 01:20-02:15 | Files v3 polish | usable Finder-style file workflows and recovery |
| S3 | 02:15-03:10 | Applications/catalog | safe app discovery, details, launch and associations |
| S4 | 03:10-04:00 | Software Center | searchable inventory and honest mutation boundaries |
| S5 | 04:00-04:50 | Dock/visual system | branded, aligned, responsive shell surfaces |
| S6 | 04:50-05:40 | Session lifecycle | startup, logout, restart, shutdown and recovery evidence |
| S7 | 05:40-07:00 | Integration/release | reboot validation, evidence pack, squash merge and cleanup |

If a slice takes longer than its timebox, preserve the branch and evidence,
record the failing check, and move to the next slice whose inputs are stable.
Return to the fix during S7 only if it can be completed and retested safely.

## S0 — establish a reproducible baseline (00:00-00:30)

1. Read the active goal, `docs/ROADMAP.md`, `docs/QUALITY_GATES.md`, and the
   current validation note.
2. Inspect `git status --short --branch`; stop if unrelated worktree changes
   would be mixed into the sprint.
3. Fetch `origin/main` and the current integration branch without rewriting
   local history.
4. List open GitHub PRs and identify which are merged, superseded, draft, or
   still a dependency of the integration PR.
5. Create or switch to the focused `codex/<milestone>-<slice>` branch from the
   synchronized parent.
6. Verify the FreeBSD release, VM identity, separate validation checkout,
   install prefix, and available graphics path.
7. Run the baseline automated gate before changing code: Qt tests, QML surface
   contract, session scripts, first-party app self-tests, and build smoke.
8. Save the baseline commit and test count in the sprint evidence note.

**S0 checkpoint:** the starting tree is clean, the branch parent is known, the
VM lane is reachable, and a failing pre-existing test is distinguished from a
new regression.

## S1 — Desktop surface and icons (00:30-01:20)

9. Confirm `Desktop` projection data is home-bound and rejects traversal or
   symlink escapes.
10. Create the Desktop directory when it is absent and expose a useful empty
    state when it has no entries.
11. Create a disposable folder and UTF-8 text file and verify the watcher or
    refresh path sees both without restarting the shell.
12. Render folder, text-file, application, unknown-file, and fallback-logo
    icon states with readable labels.
13. Make icon selection keyboard-addressable and return focus to the desktop
    surface after context-menu dismissal.
14. Verify Enter/double-click opens folders in Files and files through the
    association chooser.
15. Verify the context menu exposes Open, Open With, Rename, Delete, and
    Properties with one consistent destructive label.
16. Test rename and Delete against a disposable item, then verify the home
    boundary is still enforced.
17. Drag two icons, confirm bounded coordinates and nearest-free-cell snapping,
    restart the shell, and verify persisted positions.
18. Exercise Appearance reset and confirm the default layout returns.
19. Add or update focused controller/QML tests for any uncovered action.
20. Sync the branch to the VM, rebuild/install, and run the targeted desktop
    validation plus the full QML surface contract.
21. Capture a screenshot or sanitized observation if interactive noVNC is
    available; otherwise mark visual acceptance `OPEN`.
22. Commit S1, push it, open/update the draft PR, and attach the evidence.

**S1 checkpoint:** Desktop icons are live, safe, actionable, and persistent;
an unavailable noVNC console is the only reason the manual gate remains open.

## S2 — Files v3 and recovery workflows (01:20-02:15)

23. Open Home, Desktop, Documents, and Trash from the sidebar and confirm the
    current path remains visible and bounded.
24. Create a folder and a text file from the Files UI; verify both appear
    immediately in the active directory and desktop projection where relevant.
25. Rename the folder and file, then reopen them to confirm the model refreshes.
26. Switch list and grid/tile views without losing the current path or query.
27. Sort by name, modified time, and type in both ascending and descending
    order; verify directory-first tie behavior.
28. Search from the current home-scoped root and confirm empty and error states
    are understandable.
29. Open a text file with Open With and verify the chooser filters by the
    known MIME type or extension.
30. Edit and save the file in Northstar Text Editor using an atomic,
    user-owned save; reopen it from Files and verify the contents.
31. Set a user-scoped association, reopen the file, change the association,
    then use Forget Default and confirm the chooser returns.
32. Delete a disposable file and folder using the single `Delete` action.
33. Open Trash, inspect the deleted item, restore it, and verify its original
    path and contents.
34. Navigate a mounted non-pseudo volume and confirm the UI communicates its
    read-only boundary instead of pretending mutation succeeded.
35. Run the Files unit tests, QML surface test, and targeted installed-shell
    smoke after rebuilding the VM checkout.
36. Commit S2, push it, and update its draft PR with exact mutation/recovery
    evidence and any deferred association behavior.

**S2 checkpoint:** the core file loop is create → browse → edit → associate →
delete → restore, with no accidental Firefox default and no out-of-home path.

## S3 — Application discovery and launch safety (02:15-03:10)

37. Refresh XDG `.desktop` discovery and confirm source, desktop ID, icon, and
    launchability metadata are visible.
38. Refresh Northstar `.app` bundle discovery and verify executable, icon,
    source/package, and revision provenance checks.
39. Test a known-good first-party bundle and a known-good desktop entry.
40. Test missing executable, missing icon, malformed metadata, and an
    unavailable application; each must fail visibly and safely.
41. Confirm arbitrary `Exec` text is not evaluated through a shell and only
    validated executable/path arguments reach the launcher.
42. Search the application overview and Software Center catalog by name and
    generic name; confirm results update after refresh.
43. Open application details and verify version, source, availability, and
    unsupported-state messaging.
44. Launch a known application from the overview, Dock, Files-to-Apps flow,
    and Software Center details where the catalog marks it launchable.
45. Verify a non-launchable catalog entry has a disabled or explanatory
    action, not a dead button.
46. Reopen the launched app from the running-app strip and test focus,
    minimize, restore, and close tracking.
47. Run discovery/controller tests and first-party Welcome/Text Editor
    self-tests with the documented executable paths.
48. Commit S3, push it, and record the safe-catalog and launch evidence.

**S3 checkpoint:** the catalog is useful without becoming an untrusted command
executor, and the first-party file-association loop is demonstrable.

## S4 — Software Center inventory and mutation boundary (03:10-04:00)

49. Open Software Center from the menu, Apps surface, and any Dock shortcut.
50. Verify installed package count, search, clear-query, and empty-result states.
51. Verify application results are distinguishable from package inventory and
    expose details rather than silently doing nothing.
52. Open package details and confirm version, repository/source, installed
    state, and provenance fields are understandable.
53. Exercise install/remove/update controls only as preview or intent actions
    unless the signed repository and authorization gates are actually present.
54. Confirm unsupported mutation paths explain why they are unavailable and
    do not alter the host package database.
55. Confirm refresh has visible feedback and does not lose the current query.
56. Run the Software Center QML contract, package catalog tests, and any
    read-only package smoke available on FreeBSD.
57. Update `docs/M4_SOFTWARE.md` if the behavior or trust boundary changed.
58. Commit S4, push it, and update the PR with the exact read-only evidence.

**S4 checkpoint:** users can discover and understand software; no mutation is
claimed until signed publication, authorization, rollback, and native package
evidence are complete.

## S5 — Dock, branding, and visual coherence (04:00-04:50)

59. Verify the official Northstar logo asset is available in every intended
    scale and has a safe fallback when raster loading fails.
60. Verify the logo is used for the menu button, login/desktop background,
    Welcome, Files, Software Center, dialogs, and empty states where intended.
61. Confirm the desktop background scales to the available VM area without
    covering or shrinking the usable surface unexpectedly.
62. Confirm the top bar uses the logo affordance, correct title, clock,
    display label, and accessible hover/active state.
63. Confirm the bottom bar spans the available width and all shortcuts share
    one baseline, including Files, Apps, running apps, and Refresh.
64. Verify no Terminal auto-start remains unless explicitly requested by the
    session configuration.
65. Test Dock launch, active indicator, focus, minimize/restore, close, and
    overflow behavior with several windows.
66. Test light/dark appearance, settings navigation, menu sizing, dialog
    clipping, keyboard activation, and the VM's larger resolution.
67. Check the Software Center, Welcome, Settings, Files, and association
    surfaces for clipped text, dead controls, or inconsistent destructive
    labels.
68. Run QML compilation and surface-contract tests after any visual change.
69. Commit S5, push it, and document any visual issue that still needs noVNC.

**S5 checkpoint:** the shell reads as one Northstar product, controls align at
the actual display size, and known interaction surfaces have no dead buttons.

## S6 — Session lifecycle and diagnostics (04:50-05:40)

70. Verify the supported login/start path starts exactly one supervised
    Northstar session.
71. Confirm the environment exports the actual Wayland socket and that the
    shell does not rely on SSH-only variables.
72. Confirm the session status page reports supervision, shell PID,
    compositor PID, Wayland display, restart count, and platform accurately.
73. Exercise confirmed logout and verify only the owned user session exits.
74. Exercise graceful restart and confirm the VM returns to the expected login
    or greeter state without stale user processes.
75. Exercise shutdown only when the VM operator explicitly intends to stop it;
    record the confirmation and resulting state.
76. Trigger a bounded shell restart or inspect a controlled failure fixture and
    verify restart count, events, and terminal failure diagnostics.
77. Verify duplicate-session prevention leaves the active session untouched.
78. Inspect sanitized logs for secrets, unbounded restart loops, orphaned
    qterminal/shell processes, and misleading success messages.
79. Run the session entry-point, supervisor, shell-smoke, and restart-smoke
    checks using the actual current environment.
80. Commit S6, push it, and record lifecycle ownership and recovery evidence.

**S6 checkpoint:** the session is observable and bounded; lifecycle actions do
not become an accidental host-control API.

## S7 — Final validation, promotion, and cleanup (05:40-07:00)

81. Rebuild the final combined source in the separate validation checkout and
    install it into `/home/northstar/.local`.
82. Run the complete automated gate: Qt tests, QML surface test, shell smoke,
    session scripts, and first-party self-tests.
83. Run compiled offscreen shell startup smoke and distinguish expected
    Layer Shell notices from real QML errors.
84. If noVNC is available, perform a short end-to-end acceptance pass: login,
    logo/background, Desktop, Files, association/edit/save, Trash restore,
    Apps/Software Center, Dock, Settings, logout.
85. If noVNC is unavailable, record the manual items as `OPEN`; do not mark
    them passed from SSH or headless output.
86. Gracefully reboot the VM and repeat the minimum login → Desktop → Files →
    app path to catch stale installs and autostart regressions.
87. Capture source commit, VM checkout, install prefix, FreeBSD version,
    commands, counts, screenshots/log excerpts, and explicit graphics limits.
88. Convert every failure into either a focused fix or a documented follow-up;
    rerun the affected test and then the full automated gate.
89. Run `git diff --check`, inspect the final diff, and confirm no generated
    archives, secrets, VM state, or unrelated changes are included.
90. Update `docs/ROADMAP.md`, `docs/QUALITY_GATES.md`, and the validation note
    with current evidence rather than optimistic status language.
91. Mark only validated draft PRs ready; keep PRs draft when a required manual
    gate is still open.
92. Squash-merge the completed slice PRs in dependency order and verify the
    resulting `main` commit on GitHub.
93. Fetch/prune, switch local `main` to the verified remote commit, and confirm
    a clean worktree.
94. Delete only feature refs whose PRs are verified merged; preserve PR records
    for review history and leave unrelated/open work untouched.
95. Create the next `codex/` branch from clean `main` with a one-sentence
    objective and the exact next validation command.
96. Leave a handoff containing merged PRs, squash commits, open gates,
    deferred DRM/KMS limitations, and the next three product slices.

**S7 checkpoint:** the result is either cleanly promoted with evidence or
cleanly parked with the exact blocker and no false release claim.

## Fast failure policy

- A compile or unit failure blocks promotion of that slice but does not block
  documentation, evidence, or another independent slice.
- A VM install failure blocks manual acceptance of the affected slice; retain
  the source archive and command output, repair the validation checkout, and
  rerun before claiming success.
- A noVNC/browser availability failure leaves the manual gate `OPEN` and
  permits headless work only.
- A graphics limitation is recorded as a limitation, never worked around in
  the release report by relabeling nested X11/pixman as DRM/KMS.
- A GitHub authentication or merge failure pauses cloud promotion, but local
  commits and validation evidence remain safe; retry the same operation after
  verifying the remote state instead of force-pushing `main`.
