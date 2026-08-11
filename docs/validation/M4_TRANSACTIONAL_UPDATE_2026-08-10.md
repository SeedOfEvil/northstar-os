# M4 transactional update and rollback validation - 2026-08-10

## Scope

PR74 connects the independently verified update broker to a fixed root-only
transaction runner, ZFS boot-environment helper, repository-scoped package
mutation, post-update verification, failure recovery, and explicit rollback.

## Deterministic evidence

- QML surface contracts pass with verified-update and rollback controls.
- The root-isolated transaction smoke creates the bounded boot environment
  before package mutation.
- Successful target versions are verified and recorded as rollback eligible.
- An injected package failure activates the pre-update environment and records
  rollback as scheduled.
- The home-data sentinel survives successful, failed, and rollback paths.

## Native immutable evidence

- The separate commit archive build completed all 262 Ninja steps on
  NSTAR-DEV01. All 22 CTest targets, QML contracts, broker smoke, and
  root-isolated transaction smoke passed after correcting one stale status-text
  assertion.
- CPack generated native `northstar-0.0.9` and `northstar-0.1.0` packages with
  origin `x11/northstar` from the same install tree.
- A signed revision-74 repository published `northstar-0.1.0`; catalogue digest
  `b4742125b3c00208d73cb7b0b6ef2bde6563fa710b4aa8f004b544fccf467b04`
  and signing fingerprint
  `4c33c5b07f363e1211a4f89a9ead9c9cbb8a39185ed7e0ed1f52608f5607a924`
  were independently verified by the broker.
- The protected transaction created
  `northstar-before-development-r74-1707062` before upgrading the real system
  package from `0.0.9` to `0.1.0` and verifying the target version.
- Explicit rollback activated that environment. After reboot, `pkg info`
  reported `northstar-0.0.9` and the home sentinel remained unchanged.
- Reactivating `default` and rebooting restored `northstar-0.1.0`; the home
  sentinel still remained unchanged.
- The proven disposable boot environment was then destroyed and the VM reset
  to `northstar-0.0.9`, leaving a real signed `0.1.0` update pending for manual
  Software Center acceptance.
- Initial manual acceptance exposed a Qt application-specific configuration
  path mismatch. Policy and metadata discovery now use the documented stable
  `~/.config/northstar` namespace; all 22 tests and QML contracts passed again
  before redeployment and controlled shell restart.
- The next manual attempt exposed a missing graphical PolicyKit agent: `pkexec`
  could not open `/dev/ctty`, so no package mutation or boot environment was
  created. The session now owns `lxqt-policykit-agent`, Software Center tracks
  the protected process through completion, and cancelled or failed requests
  report an explicit result instead of appearing frozen. All 22 CTest targets,
  the session-supervisor test, and QML contracts passed in a clean VM archive
  build before redeployment.
- Manual acceptance then exposed a repeated-refresh notification latch. Commit
  `eef25c9b2af93858dc54f9cca22a320b12afad94` gives `refreshing` its own notifier;
  all 22 CTest targets passed before publishing immutable repository revision
  76 and package `northstar-0.1.2` (package SHA-256
  `c52cd199e4d23f905e639c07183c32cdeb65fc8e68d210eb2b0ab21f6862d455`).
- The isolated FreeBSD `pkg` client accepted revision 76, catalogue SHA-256
  `65610102d7ce8026c6922d2f8372590709f04f59d9bcd38b6751780d76845b5b`,
  metadata SHA-256
  `fdbb91cb9a871a6a7d1c5a6a53084909cd0617b6165d5e266cb200b412ca521e`,
  and fingerprint
  `5225f94b43b4e41138ce0c71409d34bf41b8ea8c8bf474a58d224fe401cf836f`.
- Interactive noVNC acceptance completed the authorized transaction after the
  PolicyKit administrator prompt and upgraded the installed Northstar package
  from `0.0.9` to `0.1.2`. Repository policy and publication verification
  remained green and repeated Refresh operations completed normally.
- The post-transaction output was functionally correct but clipped by a fixed
  72-pixel status card. The follow-up presents explicit success/failure state
  and scrollable multiline verification output.
- Canonical VM deployment hygiene is now repository-owned through a root-owned
  schema-2 manifest, strict read-only auditor, retention/quarantine contract,
  and branch/PR handoff procedure.

## Manual noVNC acceptance

Verified update confirmation, PolicyKit authorization, package mutation to
`0.1.2`, repository refresh, window movement, and the core 1280x800 flow pass.
Explicit rollback/reboot evidence and the final scrollable completion-result
visual check remain before PR promotion.
