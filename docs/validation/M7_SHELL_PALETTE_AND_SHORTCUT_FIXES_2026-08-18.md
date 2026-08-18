# Shell palette and shortcut defect fixes — 2026-08-18

PR #97 was validated from `codex/m7-shell-palette-and-shortcut-fixes` on the
supplemental NSTAR-DEV01 FreeBSD 15.1-RELEASE-p2 Proxmox/noVNC lane.

Both defects were found while validating Settings v2 (PR #96) and were
deliberately left out of that slice to keep it scoped. Both pre-date it: the
palette defect dates to the unified-search slice (PR #70), and the shortcut
defect is the same one already fixed for the Text Editor in `b8dab58`.

## Candidate

- Product candidate: `74d0f8ced292dc872217be776fc9ac49208b9b42`.
- Canonical build directory: `/home/northstar/builds/pr97-74d0f8c`.
- Installed development prefix: `/home/northstar/.local`.
- The canonical checkout was clean at the candidate commit.

## Defects fixed

### Unified search had no selection highlight

`src/shell/SearchOverlay.qml` bound `lunar.borderStrong` (line 103) and
`lunar.selection` (line 220). `LunarPalette` defines neither, so both evaluated
to `[undefined]`, the overlay border and the selected-result highlight rendered
with no colour, and every shell start logged:

```
qrc:/Northstar/Shell/SearchOverlay.qml:220:29: Unable to assign [undefined] to QColor
qrc:/Northstar/Shell/SearchOverlay.qml:103:9: Unable to assign [undefined] to QColor
```

Both were repointed at tokens already used elsewhere for the same jobs:
`border`, the stronger of the two border weights, and `accentSoft`, which
Dock, Desktop, Quick Settings, and Settings all use to mark the selected item.
No new palette token was introduced for a single call site.

### Files clipboard and undo alternates were dead

`src/shell/FileBrowserWindow.qml` bound Cut, Copy, Paste, and Undo with
`sequence: StandardKey.X`. Each of those maps to more than one platform
sequence, and `sequence:` registers only the first.

### Audit

A repository-wide audit found no further instances of either defect. Every
`lunar.X` and `lunarPalette.X` reference in `src/` and `apps/` was cross-checked
against the properties `LunarPalette` defines: exactly two were undefined, both
in `SearchOverlay.qml`. Every `sequence: StandardKey.` was located: exactly the
four in `FileBrowserWindow.qml`. Plain string sequences such as `"Ctrl+T"` and
`"Escape"` are single bindings and were correctly left alone.

## Regression guards

Neither defect had any check that would have caught it, which is why both
survived on `main`. `tests/unit/test-qml-surfaces.sh` now enforces:

- every `lunar.X` / `lunarPalette.X` reference must resolve to a property
  `LunarPalette` actually defines;
- no QML file may use `sequence: StandardKey.`, because the list form is
  correct for single-binding keys too.

Both guards were verified by reintroducing each original defect and confirming
the contract fails, then reverting and confirming it passes:

```
FAIL: LunarPalette defines no 'borderStrong', but a QML surface uses it
FAIL: use sequences: [StandardKey.X] so every platform binding is registered
```

A stale contract that pinned the *defective* single-sequence form in
`FileBrowserWindow` was corrected to the working form.

## Automated evidence

- Clean FreeBSD build, all 377 targets.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr97-74d0f8c --output-on-failure`
  — **31/31 suites passed, 0 failed**, 11.83 s.
- `sh tests/unit/test-qml-surfaces.sh` — passed on FreeBSD, including both new
  guards.
- `git diff --check` — exit 0.
- Installed with `cmake --install` to `/home/northstar/.local`.
- Starting the installed shell offscreen emits **zero `qrc:` diagnostics of any
  kind**: no undefined-QColor, no QML Shortcut warnings, no binding loops. The
  same command against the previous build emitted two of the first and four of
  the second.
- `tests/integration/test-shell-session.sh` — passed, `live shell is PID 10320`.
- `tests/integration/test-shell-session.sh --restart` — passed,
  `shell-only restart created PID 11676`. The qterminal client count was 0
  before and after, so this run carries no evidence about client survival.

`make validation-deployment-audit` is **not claimed to pass**; DEV01's
root-owned manifest still describes PR74/r78, unchanged from PR #95 and #96.

## Deferred gates

Unchanged: physical Intel/AMD DRM/KMS acceptance, native compositor quality,
and multi-display evidence remain open. The Proxmox scfb/pixman VM remains
supplemental product evidence only.

## Interactive acceptance

The user confirmed both fixes on the restarted session: the unified-search
selection highlight renders correctly, and the Files clipboard and undo
shortcuts work.

## Separate open defect, not addressed here

While checking this PR the user found that **`Ctrl+K` opens unified search only
once per shell start**; after the overlay is closed the shortcut does not
reopen it. This is not caused by PR #97, whose entire change to
`SearchOverlay.qml` is the two colour bindings above, and it is not caused by
the shortcut change, which touches only `FileBrowserWindow.qml`.

An initial hypothesis, that closing the overlay leaves the application with no
active window and so kills key delivery for every `Qt.ApplicationShortcut`, was
**disproven**: the user reports the other shell shortcuts continue to work
after the overlay is closed. The cause is therefore specific to `Ctrl+K` or to
reshowing the overlay window, not to global key delivery.

`closeSearch()` calls `hide()` and re-activates nothing, and both the overlay
and the shell panel are `Qt.Tool` windows, so remapping a hidden `Qt.Tool`
window under the nested Wayfire compositor is one candidate worth examining
first. This is tracked as its own focused change.

Note for whoever picks it up: DEV01 has no key-injection tool installed and
Wayfire's `stipc` plugin is not enabled in `~/.config/wayfire.ini`, so the
defect cannot currently be reproduced non-interactively. Enabling `stipc`
would allow synthetic input, but it changes the session configuration and
requires a compositor restart.
