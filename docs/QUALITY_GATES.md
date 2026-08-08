# Quality gates

Quality gates are evidence requirements, not aspirations. The author records the exact command and environment; reviewers check that the evidence matches the change.

## Pull request gate

Every pull request must satisfy:

- scope matches the issue and milestone;
- documentation and ADRs reflect architectural changes;
- new behavior has tests at the appropriate layer;
- native FreeBSD checks are included when the change touches FreeBSD behavior;
- no unrelated formatting churn is included;
- all build inputs remain pinned;
- new external licences and notices are recorded;
- no secrets, signing keys, images, package repositories, or downloaded source archives are committed;
- rollback or revert behavior is stated.

Hold a change when it depends on an unpinned dependency, requires broad root access, changes the FreeBSD base without a compelling reason, couples the shell to undocumented Wayfire internals, adds Apple-owned assets, or breaks the clean-build/clean-install lane.

## Milestone gates

| Gate | Required evidence |
| --- | --- |
| M0 host | Exact FreeBSD 15.1 amd64, clean VM bootstrap, idempotent second run, package versions, Wayland and Xwayland smoke tests |
| M1 shell | Multi-monitor Layer Shell behavior, unprivileged launch, app launch, shell-only restart, Qt unit tests |
| M2 session | Exactly one session, controlled lifecycle, service restart, launch identity, bounded notification feedback, sanitized logs |
| M3 desktop | File operations, settings persistence, user-scoped file associations, home search, mounted-volume locations, overview, Files-to-Apps drag-and-drop, project-owned menu, validated app-bundle provenance |
| M4 update | Signed repository, N-1 to N upgrade, pre-upgrade `bectl` environment, rollback, home-data preservation |
| M5 image | Clean builder, pinned inputs, checksums, UEFI GPT/root-on-ZFS install, first boot, update/rollback |
| M6 alpha | Supported VM and physical hardware matrix, diagnostics, crash recovery, shutdown, application coverage |

## Reproducibility gate

Release artifacts are blocked unless:

- `packaging/manifests/upstream.lock` has no unresolved values;
- each external source has a version or commit and checksum;
- package versions are captured from the selected repository;
- the project commit and build toolchain are recorded;
- artifact SHA-256 is generated after assembly;
- a clean builder can reproduce equivalent package contents.

## Security gate

Reviewers must verify privilege boundaries, authorization, secret handling, log redaction, package/repository signing, and CI runner isolation. Public pull requests must not execute on a persistent privileged release builder. Release builds run only from protected branches or manually approved workflows in disposable environments.

## Release gate

No alpha image is published until install, login, Qt/Wayland launch, Xwayland launch, browser launch, file management, settings, update, rollback, diagnostics, shell crash recovery, and clean shutdown have current evidence on the declared hardware matrix.
