# Web application bundles: browser-launch baseline

Northstar can package a website as a named, icon-bearing `.app`, install it
through Files, show it in Applications, and remove it through the existing
user Trash workflow. This slice is a browser shortcut bundle, **not** an
offline PWA runtime, a macOS app, an embedded web engine, or an isolated profile.

## What happens when opened

Northstar validates the current web manifest and asks `/usr/local/bin/firefox`
to open one HTTPS URL using `--new-window`. URL and flag are separate process
arguments; no shell or bundle-supplied executable runs. Firefox must already
be installed; Northstar does not download it. A successful process launch is
not evidence that the website loaded or is reachable.

The website address, derived origin, and these fixed policies appear before
installation and in Application Information:

- Internet access is required; Northstar provides no offline guarantee.
- Cookies, login sessions, history, cache, and storage share the Firefox profile.
- Firefox handles site permissions; Northstar grants none on the site's behalf.
- The initial origin is a disclosure, not a navigation/network allowlist.
  Links, redirects, scripts, and subresources can contact other origins.
- Removing the bundle does not delete Firefox data, revoke permissions, log
  out of the website, or close an already-open browser window.

The browser retains its normal address bar and identity. It is not relabelled
as a separate native application or promised a separate running Dock identity.
Web bundles cannot register document extensions or receive files through the
Northstar Open With / drag-to-app path. Browser uploads remain normal browser
actions that the user initiates there.

## Package the sample

Build the updated `northstar-app` and `northstar-shell` from this branch first.
Older launchers reject web manifests because they have no native Executable
field; they do not silently treat them as native apps.

```sh
cmake -S . -B build -G Ninja
cmake --build build --target northstar-app northstar-shell
cmake -S apps/samples/web-demo -B build-web-demo \
  -DWEB_DEMO_REVISION="$(git rev-parse HEAD)"
mkdir -p "$HOME/Downloads/NorthstarWeb"
chmod 700 "$HOME/Downloads/NorthstarWeb"
build/src/launcher/northstar-app package build-web-demo/recipe.json \
  "$HOME/Downloads/NorthstarWeb/WebDemo.app"
build/src/launcher/northstar-app inspect "$HOME/Downloads/NorthstarWeb/WebDemo.app"
```

The sample uses the project's existing Welcome icon and licence, root VERSION,
and an explicit source revision. It links to `https://example.org/` solely to
test opening a website. Packaging copies no website content and does not claim
ownership of, redistribute, or grant a licence for remote content. The bundle
licence covers only the local wrapper assets. No network access is needed to
create or inspect the bundle.

## Recipe version 2

Use the native packaging recipe's identity, version, icon, categories, licence,
and provenance fields. Set `schemaVersion` to `2`, omit `executable`, and add:

```json
"web": {
  "URL": "https://example.org/",
  "Browser": "firefox",
  "Network": "required",
  "Storage": "shared-browser-profile",
  "Permissions": "browser-managed"
}
```

All five case-sensitive fields are required; only URL is configurable. The
other values describe the implemented behavior and cannot be changed to
claim isolation or offline support. Unknown fields, browser paths, flags,
credentials in URLs, missing hosts, non-HTTPS schemes, raw whitespace/control
characters, malformed escapes, and backslashes are rejected. URL length is
limited to 200 characters before and after encoding. International domain
names display in their encoded form. URLs may include queries/fragments;
never embed secrets or access tokens in a distributable recipe.

The generated plist has `WebApplication` with the same five fields. It must
not also specify `Executable` or `DocumentExtensions`. The Executable directory
is empty. Icon, licence, ownership, private staging, tree-size limits, duplicate
refusal, and Trash behavior reuse the [native packaging kit](APP_PACKAGING_KIT.md).
Version 1 native recipes remain unchanged.

## Validation / acceptance

```sh
cmake --build build --target northstar-app northstar-shell \
  test-applicationbundlepackager test-applicationbundlecatalog
QT_QPA_PLATFORM=offscreen ctest --test-dir build \
  -R 'northstar-(applicationbundlepackager|applicationbundlecatalog|web-bundle-cli)$' \
  --output-on-failure
sh tests/unit/test-qml-surfaces.sh
QT_QPA_PLATFORM=offscreen build/src/shell/northstar-shell --qml-self-test
```

Automated tests inspect exact browser arguments without starting Firefox or
contacting a website, reject dangerous/misleading recipes, revalidate changed
web metadata at launch, and cover the existing install/remove flow. The user
confirmed the staged sample's Files installation, website/policy disclosures,
Firefox launch, Application Information and removal on the physical Intel
laptop, then approved merging on 2026-09-05. Required GitHub CI precedes merge.
No image rebuild is needed. This acceptance covers the browser-launch slice,
not the deferred profile isolation or offline/PWA capabilities below.

Initial automated evidence: native FreeBSD 15.1 amd64 with Qt 6.11.1 passed all
three focused CTest entries, QML surface contracts, and the shell runtime
self-test, including bounded long-text install and information dialogs. These
checks do not contact a website and do not substitute for graphical acceptance.

## Deferred work

Profile isolation, dedicated windows without browser chrome, permission
brokering, reviewed app catalogues, offline verification, service-worker
updates, notifications, signing and publisher trust require separate designs
and acceptance. This is the first browser-launch slice of Applications v2,
not completion of the broader PWA milestone.

Browser invocation reference: [Mozilla command-line parameters](https://firefox-source-docs.mozilla.org/browser/CommandLineParameters.html).
