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

Pending commit archive rebuild, full test gate, native package generation,
signed-channel client smoke, and installed-artifact deployment.

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
