# ADR 0010: One-time first-boot provisioning boundary

Status: Accepted

## Context

A production Northstar image cannot ship a reusable administrator password or
depend on an administrator that does not exist yet. The graphical setup flow
must create the first administrator without exposing its password in a file,
process argument, environment variable, or log.

## Decision

Production images enter a dedicated `northstar-setup` SDDM session only while
a root-owned, mode-0600 pending marker exists. PolicyKit permits the fixed
`northstar-first-boot-provision` executable only from an active local session;
the helper independently verifies that the caller is `northstar-setup`, owns a
mode-0600 bounded request, and is operating against the pending installation.

The request contains only a protocol version, account name, display name,
locale, timezone, keyboard layout, and explicit administrator confirmation.
The password is delivered once over the helper's standard input and cleared by
the GUI after the process starts. The helper creates one wheel administrator,
copies the package-owned compatibility-session configuration into that fresh
home, and creates one root-owned mode-0440 sudoers drop-in naming only that
administrator. It then applies only allowlisted regional values, locks the
temporary setup identity, removes its SDDM autologin drop-in, writes a
root-owned completion marker, and removes the pending marker. A completed
installation rejects every later run. Any pre-completion failure removes both
the partial account and its sudoers policy.

Development-autologin images retain their existing explicit development user
and do not enter the production setup session.

## Consequences

- No factory credential exists and password material is absent from durable
  setup state.
- The temporary passwordless setup identity is local, non-administrative,
  useful only while the protected pending marker exists, and sealed on success.
- The UI and bounded helper can be tested on DEV01 without mutating an account
  database; integrated boot behavior remains deferred to the M5 Installer
  Release Candidate image checkpoint.
- An interrupted provisioning attempt removes a newly created account before
  completion, removes its authorization policy, and leaves setup pending for a
  safe retry.

## Alternatives considered

Requiring an existing administrator is circular on a new installation.
Shipping a default administrator password creates a reusable credential.
Running the QML wizard as root gives a broad graphical process unnecessary
privilege. Passing the password through a request file, argv, or environment
leaks it into observable or durable state.

## Validation

Unit and shell contracts cover profile bounds, forbidden secret fields,
one-shot password delivery, rollback on failure, completion sealing, QML
loading, production-image setup identity/autologin wiring, and separation from
the explicit development-autologin lane. Final SDDM, restart, login, and image
behavior is validated once at the M5 Installer Release Candidate checkpoint.
