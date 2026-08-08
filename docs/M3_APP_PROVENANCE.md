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

## Evidence

Run the native unit suite from the FreeBSD development VM:

```sh
cd /home/northstar/src/northstar
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

The unit suite covers valid provenance propagation and rejects missing or whitespace-padded provenance records.
