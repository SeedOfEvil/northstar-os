# Application actions — 2026-08-19

PR #110, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Qt 6.11.1) at
commit `30aec35`, built in `/home/northstar/builds/pr110-30aec35` with the
project's canonical `-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

## What was missing

The desktop entry specification lets an application declare additional
actions — the entries a jump list or a right-click menu offers beside the
application itself. Three are installed on this machine already:

| Application | Action | Command |
| --- | --- | --- |
| Firefox | Open a New Window | `firefox -new-window` |
| Firefox | Open a New Private Window | `firefox -private-window` |
| QTerminal | Drop-down Terminal | `qterminal --drop` |

Northstar ignored all three. The parser read the `[Desktop Entry]` group and
discarded every other group in the file, so the action definitions were never
seen. Right-clicking an application offered only the dock's own commands.

## What this adds

Actions appear at the top of the right-click menu in both the dock and the
application overview, above the pin and ordering commands, the way a jump list
sits above a launcher's own entries.

Order comes from `Actions=` rather than from where the groups happen to appear
in the file. The installed Firefox file happens to list `NewWindow` first in
`Actions=`, and a menu that reordered them would be harder to use twice.

## What is refused, and why that is right

An action is dropped rather than shown when it cannot work:

- no `Name`, so there is nothing to put in a menu
- no `Exec`, so there is nothing to run
- named in `Actions=` with no matching group, so it was never defined
- restricted by `OnlyShowIn` or `NotShowIn` to a desktop that is not this one

In every case the application itself stays perfectly usable. Dropping one
action silently is right; refusing to list the application because one of its
actions is malformed would not be.

Asking for an action an application does not declare is refused outright
rather than falling back to an ordinary launch. Someone asking for a private
window would not want a normal one instead, and a fallback would make that
mistake silently.

## Verified against this machine, not only fixtures

The unit tests use written fixtures, which prove the parsing rules but not
that any real file matches them. A read-only probe ran the catalog against the
machine's own `/usr/local/share/applications`:

```
firefox -- Firefox Web Browser
    NewWindow | Open a New Window | firefox -new-window
    NewPrivateWindow | Open a New Private Window | firefox -private-window
qterminal -- QTerminal
    Dropdown | Drop-down Terminal | qterminal --drop
applications: 8  with actions: 2
```

Each action resolves to the command its file declares. The probe launches
nothing.

## Automated evidence

- Clean build, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr110-30aec35 --output-on-failure`
  — **37/37 suites passed, 0 failed**.
- Four new cases in `northstar-applicationcatalog`: actions read in the order
  `Actions=` gives rather than group order, the four kinds of unusable action
  dropped while the application survives, an action's command built from its
  own `Exec` with field codes handled as for an ordinary launch, and an
  undeclared action refused for both a known and an unknown application.
- The probe output above.
- `northstar-shell --qml-self-test` on the installed binary — exit 0, no QML
  warnings.
- The suite was re-run after `cmake --install` into `~/.local` and still
  reported 37/37.
- `git diff --check` — exit 0.

### Two fixture mistakes worth recording

The first run of these tests failed on all four cases with
`catalog.reload() returned FALSE`. `reload()` reports whether the catalog
*changed*, and the constructor has already loaded, so asserting it returned
true was simply wrong about the interface. The same fixtures also wrote `%%U`
where `%U` was meant, on a reflex from format strings that `QStringLiteral`
does not have.

Neither was a product defect. They are recorded because the tests were written
before the interface was checked, and both would have been caught by reading
an existing case first.

## Not claimed

The menus themselves are not covered by the QML surface tests added in PR
#109, which drive the settings control only. Whether the action appears in the
menu, and whether choosing it calls the launcher, rests on the interactive
walkthrough below. Extending those tests to the dock and overview menus would
close that, and is not in this change.

Nothing here tests that Firefox actually opens a private window when told to;
that is Firefox's behaviour. What is verified is that Northstar runs the
command the file declares.

## Interactive acceptance

Accepted by Hector on 2026-08-19: "success pass". The walkthrough covered both
Firefox actions in the dock menu above a separator, opening a new private
window, the QTerminal action, the same actions in the application overview,
and an application with no actions showing a plain menu with no stray
separator.

Status: **accepted**.
