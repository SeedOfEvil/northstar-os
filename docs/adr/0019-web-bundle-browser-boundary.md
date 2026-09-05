# ADR 0019: Web bundles delegate to the installed browser

Status: Accepted after focused Intel validation, 2026-09-05

## Decision

Add a mutually exclusive `WebApplication` alternative to the native
`Executable` manifest field. Its five fields disclose URL, Browser, Network,
Storage and Permissions. Require an HTTPS URL without embedded credentials,
Firefox, required network, shared browser profile and browser-managed
permissions. The normal launcher passes one validated URL to the fixed
platform Firefox executable with a fixed new-window flag, never a shell.
Revalidate web manifests when launching. Reject document extensions and local
file delegation for this bundle type. Existing native bundles are unchanged.

## Trust boundary and consequences

Installation still validates ownership and the complete tree through the
unprivileged bundle installer. A displayed origin is a starting address, not
an enforcement boundary: navigation and remote code stay under Firefox's
ordinary policies. Installation neither contacts the website nor approves its
permissions. Browser profile data is shared and survives bundle removal.
Display these limits both before installation and in application information.
Provenance is descriptive and unsigned. Bundle licensing does not license
remote website content. No browser dependency is installed automatically.

## Alternatives

- Generated launcher scripts would add avoidable quoting and executable-input
  surfaces, so the manifest is data-only for this path.
- A custom web engine or separately managed profiles would create additional
  security, update, persistence and session boundaries without current evidence.
- Claiming full PWA support would overstate the browser-shortcut implementation.

## Validation

Require positive native packager/catalog/CLI tests, exact argv assertions,
negative URL and policy cases, no file delegation, changed-manifest rejection,
QML checks, and focused physical install/disclosure/launch/removal acceptance.
Follow [WEB_APP_BUNDLES.md](../WEB_APP_BUNDLES.md). Full offline/profile/PWA
support remains deferred.
