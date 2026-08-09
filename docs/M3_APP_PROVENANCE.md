# M3 application bundle provenance

The Northstar application catalog now requires every project-defined `.app` bundle to declare a small, machine-readable provenance record in `Contents/Info.plist`.

```xml
<key>Provenance</key>
<dict>
    <key>Source</key>
    <string>northstar-project</string>
    <key>Package</key>
    <string>northstar-welcome</string>
    <key>Revision</key>
    <string>development</string>
</dict>
```

`Source` identifies the producing project or source family, `Package` identifies the owning component, and `Revision` identifies the source/package revision. The values are bounded and reject leading/trailing whitespace and control characters. The catalog exposes the three fields to launcher clients and includes them in bundle search matching.

This slice establishes an auditable identity boundary for development bundles. It does not claim authenticity: cryptographic signatures, repository metadata verification, and package-manager integration belong to the M4 packaging and update work.

Validation remains unprivileged and local. The launcher still checks bundle ownership, rejects symlink and traversal escapes, requires safe file permissions, and requires an executable entry point before exposing a bundle.

## Live application discovery

The launcher watches the configured XDG application directories and the
Northstar bundle roots. A debounced refresh follows `.desktop` edits, new or
removed entries, and `.app` bundle changes without requiring a shell restart or
manual menu refresh. The catalog exposes `sourceType`, `sourcePath`, `exec`,
and a current `launchable` hint for desktop entries; bundle entries continue to
expose their validated icon and provenance fields. Launch commands still pass
through the existing tokenized Exec parser and never use a shell evaluator.

The launchable field is discovery metadata, not a signature or trust decision:
package authenticity and privileged installation remain M4 concerns.

## Evidence

Run the native unit suite from the FreeBSD development VM:

```sh
cd /home/northstar/src/northstar
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

The unit suite covers valid provenance propagation, rejects missing or
whitespace-padded provenance records, and verifies automatic refresh when a
desktop entry or bundle is added after catalog construction.
