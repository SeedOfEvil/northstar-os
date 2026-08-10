# M4 signed development repository validation - 2026-08-10

## Scope

This record covers PR #73's native Northstar package, externally signed
development repository, manifest-bound provenance, disposable client trust,
tamper rejection, and read-only Software Center presentation.

## Working-tree FreeBSD evidence

- The isolated build in
  `/home/northstar/validation/signed-development-pr73-working/build` completed
  all 262 Ninja steps on NSTAR-DEV01.
- The complete Qt/offscreen gate passed: 22 of 22 CTest targets.
- The QML surface contract passed.
- CPack generated the real `northstar-0.1.0-amd64.pkg` artifact with package
  name `northstar` and origin `x11/northstar`.
- The publisher resolved the FreeBSD release, architecture, exact quarterly
  Ports commit, installed Qt and Wayfire versions, and project revision.
- External disposable signers produced the FreeBSD catalogue and schema-2
  manifest signature; no private key appeared in the publication output.
- A root-owned isolated `pkg` client trusted and refreshed the authentic
  development repository without installing, removing, or upgrading a
  package.
- The same isolated client rejected copied catalogue archives after their
  embedded signatures were intentionally altered.
- The update-plan unit test verifies schema-2 metadata signatures and rejects
  a validly structured manifest changed after signing.

## Immutable evidence

- Commit `a16a533` was exported with `git archive` and built in the separate
  `/home/northstar/validation/signed-development-a16a533` checkout on
  NSTAR-DEV01; all 262 Ninja steps completed.
- The immutable archive passed all 22 CTest targets and the QML surface
  contract.
- CPack generated `northstar-0.1.0-amd64.pkg`; `pkg info -F` confirmed package
  name `northstar`, version `0.1.0`, origin `x11/northstar`, FreeBSD 15 amd64,
  and the declared runtime libraries.
- The isolated signed-channel smoke refreshed and exposed Northstar from the
  authentic repository, rejected altered signed catalogue metadata, performed
  no package mutation, and found no private key in the publication. Its
  catalogue digest was
  `aabe18819719f30b42f3fc556853ff6b5cd3f859aafbc1d47b8499d65754bbf3`.
- The same immutable build was installed under `/home/northstar/.local`. A
  persistent test-only publication was generated at
  `/home/northstar/validation/development-channel-a16a533`, with catalogue
  digest `66ea1b02c532ed4b6a7359f06616a545a6a5b02b9c923c484f6407cdf10baccb`
  and metadata digest
  `761a66a9fcadb3ced9d22ea651ae2f0a7b4ec6fcceeb6e30d614f4e9ced79321`.
- Its public trust and presentation files were installed under
  `/home/northstar/.config/northstar`; the disposable private key was removed
  and a filesystem audit found no private key in either location.
- The supervised shell restarted from the installed binary as PID `42855`.

## Manual 1280x800 noVNC acceptance

Pending user validation:

- Software Center remains read-only and its package inventory still refreshes;
- Update Plan shows the verified `development / northstar-development`
  channel and repository revision;
- the manifest digest is visible and package provenance lists Northstar's
  name, version, origin, and project revision without clipping;
- publication status is verified while Apply Update remains disabled;
- moving, resizing, maximizing/restoring, closing, and both themes work.

## Deferred release evidence

The validation key is disposable. Protected Poudriere production builds,
stable repository hosting, persistent offline key custody, package mutation,
boot-environment updates, and rollback remain separate release gates.
