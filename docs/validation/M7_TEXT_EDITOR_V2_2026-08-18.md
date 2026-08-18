# M7 Text Editor v2 validation — 2026-08-18

PR #95 was validated from `codex/m7-text-editor-v2` on the supplemental
NSTAR-DEV01 FreeBSD 15.1-RELEASE-p2 Proxmox/noVNC lane at 1280×800.

This is a routine M7 application slice. No installer image was assembled and
NSTAR-TEST01 was not used: the change touches no boot, installation, package
database, early service, GPT/ZFS, or rollback boundary.

## Candidate

- Product candidate, built and gated:
  `b8dab58def7a932f8f5efe6a494df275fff3f921`.
- The branch head also carries this documentation-only evidence commit, which
  changes no build input and was not rebuilt.
- The canonical checkout `/home/northstar/src/northstar` was clean on `main`
  at `a257d9f` before the handoff, so nothing needed quarantining. It was
  fast-forwarded onto the pushed branch and verified clean at the candidate.
- Canonical build directory: `/home/northstar/builds/pr95-b8dab58`.
- Installed development prefix: `/home/northstar/.local`.

An earlier commit on the same branch, `271897b`, was built and gated at
`/home/northstar/builds/pr95-271897b`. It is superseded by `b8dab58` and is
retained rather than deleted; only `pr95-b8dab58` is canonical for this
handoff. The `pr94-4ada8bb` predecessor build and both quarantine trees
(`20260817-pre-pr94-4ada8bb`, `pre-pr74-b3e1224`) are untouched.

## Automated evidence

All commands below were run on DEV01 against the candidate commit.

- The clean FreeBSD build completed all 365 targets with no warnings surfaced
  in the final link stage.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr95-b8dab58 --output-on-failure`
  — **29/29 suites passed, 0 failed**, 9.97 s.
- The rewritten `northstar-texteditorcontroller` suite reported
  **20 cases passed, 0 failed, 0 skipped**. Nothing was skipped, including the
  mode-000 refusal case, so DEV01 genuinely exercised the unreadable-file path.
- `sh tests/unit/test-qml-surfaces.sh` — passed, including the 31 new Text
  Editor v2 surface contracts.
- `sh tests/unit/test-session-script.sh` — exit 0.
- `sh tests/unit/test-session-entrypoint.sh /home/northstar/builds/pr95-b8dab58`
  — exit 0.
- `git diff --check` — exit 0; the checkout reported no modifications at the
  candidate commit.
- The candidate was installed with
  `cmake --install /home/northstar/builds/pr95-b8dab58 --prefix /home/northstar/.local`.
- The **installed** binary passed its self-test:
  `env QT_QPA_PLATFORM=offscreen "$HOME/.local/share/northstar/apps/NorthstarTextEditor.app/Contents/Executable/northstar-text-editor" --self-test`
  — exit 0.
- Loading the installed QML surface produced **zero QML warnings and zero
  errors** (`--qml-self-test`, exit 0). The only output is the offscreen
  platform's benign `raise()` notice.

### A defect the QML gate caught

Loading the surface on the live compositor at commit `271897b` printed six
`QML Shortcut: Only binding to one of multiple key bindings` warnings. New,
Open, Save, Save As, Close, and Find each map to more than one platform key
sequence, and `sequence:` binds only the first, leaving the alternates dead.
Commit `b8dab58` changes those to `sequences: [StandardKey.X]`. The warnings
are gone in the candidate.

### Shell session gates

- `tests/integration/test-shell-session.sh` — **passed**, reporting
  `live shell is PID 1538 as northstar on wayland-1`.
- `tests/integration/test-shell-session.sh --restart` — **passed**, reporting
  `shell-only restart created PID 5520`. The check also reported that it
  preserved the qterminal client count; that count was **0 before and after**,
  so this run carries no evidence about client survival.

Both shell checks were executed against commit `271897b`, not the final
candidate. The only difference between `271897b` and `b8dab58` is
`apps/text-editor/TextEditorWindow.qml` and its surface contracts, so the
shell binary is identical in both. They were not re-run on `b8dab58` because
the graphical session had ended by then, and both checks require a live
session. They cannot run over a plain SSH connection: the script requires
`WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR`, and a present Wayland socket, and it
exits with `FAIL: WAYLAND_DISPLAY is not set` without them. Both runs above
supplied the live session's own environment explicitly.

Note that `make shell-smoke` reconfigures and rebuilds a second tree at the
default `BUILD_DIR=build` inside the canonical checkout. That secondary tree
was created during this handoff and then removed; the checkout is clean and
`git status --porcelain` is empty. Invoke the integration script directly with
`NORTHSTAR_SHELL_BIN` to gate an already-built candidate.

## Deployment manifest: an honest exception

`make validation-deployment-audit` is **not claimed to pass** for this
handoff, and it was not run as a passing gate.

The root-owned `/usr/local/etc/northstar/validation-deployment.conf` is dated
2026-08-10 and still describes PR74:

```ini
schema_version=2
canonical_build=/home/northstar/builds/pr74-017fc81
source_branch=codex/m4-transactional-update-rollback
source_revision=017fc81040bb33879596b6a3dde630212e30524f
repository_revision=78
```

The canonical checkout and installed development build are now PR95/`b8dab58`,
so the manifest is stale. It was left untouched rather than rewritten: its
required schema-2 keys are package- and repository-oriented
(`package_file`, `package_sha256`, `catalogue_sha256`, `signature_fingerprint`,
`repository_path`), and this slice publishes no package and no signed
repository revision. Filling those keys for a source-only UI deployment would
mean inventing provenance, and overwriting `repository_revision=78` would
misrepresent an immutable published revision.

Reconciling the runbook's package-oriented manifest with routine source-only
UI deployments remains open process debt, tracked separately from this slice.
It does not gate this PR's product behavior, and no image rebuild is required
to resolve it.

## Deferred gates

Unchanged by this slice and still open: physical Intel/AMD DRM/KMS acceptance,
native compositor quality, and multi-display evidence. The Proxmox scfb/pixman
VM remains supplemental product evidence only. The `libEGL`/`MESA ZINK`
notices observed when starting Qt applications in this lane are expected there
and are not graphics-path evidence.

## Interactive acceptance

DEV01 was left at the SDDM greeter, so the next login starts the newly
installed binaries. Log in with **Northstar (Proxmox X11 fallback)**.

_Pending. This section is completed only after the user runs the focused
1280×800 noVNC checklist and reports the result._
