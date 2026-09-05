# Native application packaging kit

The developer kit adds `northstar-app package recipe.json Output.app`. It
assembles finished, trusted local build inputs; it does not compile or execute
them. Run it as your normal user on FreeBSD, not in a root shell.

## Quick start: graphical sample

From a Northstar source checkout with CMake, Ninja, a C++ compiler, and the
existing Qt 6 development dependencies installed:

```sh
cmake -S . -B build -G Ninja
cmake --build build --target northstar-app
cmake -S apps/samples/packaging-demo -B build-packaging-demo -G Ninja \
  -DDEMO_REVISION="$(git rev-parse HEAD)"
cmake --build build-packaging-demo
mkdir -p "$HOME/Downloads/NorthstarPackaging"
chmod 700 "$HOME/Downloads/NorthstarPackaging"
build/src/launcher/northstar-app package build-packaging-demo/recipe.json \
  "$HOME/Downloads/NorthstarPackaging/PackagingDemo.app"
build/src/launcher/northstar-app inspect \
  "$HOME/Downloads/NorthstarPackaging/PackagingDemo.app"
```

Open that folder in Files, double-click `PackagingDemo.app`, confirm its
identity and provenance, and install. Open **Northstar Packaging Demo** in
Applications: a small window says that your packaged app is running. Close it,
then use Application Information to move the user-installed app to Trash.
The original bundle in Downloads remains unchanged. Existing output is never
overwritten: use another output name when packaging again.

This sample uses the root `VERSION`, an explicit source revision, the project
licence, and the existing Welcome icon. It is not part of the installed desktop
or the system package. No desktop restart or new installer image is required.

## Recipe version 1

All fields below are required; unknown fields are rejected. Paths are relative
to the recipe file, not the current working directory.

```json
{
  "schemaVersion": 1,
  "bundleIdentifier": "org.example.MyApp",
  "displayName": "My App",
  "version": "1.0.0",
  "executable": "build/my-app",
  "icon": "assets/icon.svg",
  "categories": ["Utility"],
  "license": {"id": "BSD-2-Clause", "file": "LICENSE"},
  "provenance": {
    "source": "example-project",
    "package": "my-app",
    "revision": "explicit-source-revision"
  }
}
```

- Identity uses reverse-domain components containing ASCII letters, digits,
  hyphens, or underscores. Version uses `X.Y.Z` with an optional `-` prerelease
  or `+` build suffix. Do not use a moving label such as `latest`.
- Text values are nonempty, trimmed, at most 200 characters, without control
  characters. Categories contain 1-16 such strings.
- Licence ID is a single identifier (prefer SPDX, or a `LicenseRef-` value),
  not a compound expression. The kit checks syntax and includes the supplied
  UTF-8 licence text; it does not determine whether you have redistribution rights.
- Recipe limit: 64 KiB. Executable: 128 MiB. Icon: 8 MiB. Licence: 1 MiB.
- Inputs must be regular files owned by the packaging user, not group/world
  writable or setuid/setgid. The executable needs owner-execute permission.
  Symlink input components, traversal, absolute input paths, and special files
  are rejected. Keep the source directory stable throughout packaging.
- ELF magic or an absolute script shebang is required. This is a format check,
  **not** ABI, CPU, interpreter, dependency, or runtime compatibility proof.
  Build the app for the destination FreeBSD system. PE and Mach-O are rejected.
- Icons accept SVG XML or a PNG signature. These checks are not a sandbox or
  a complete image-decoder validation. Package trusted assets only.

The output parent must already exist, belong to you, and not be writable by
others. The kit stages privately, uses the existing installer validation,
and publishes a new `.app` directory. Failure does not publish a partial app.
Packaging does not install, launch, elevate privileges, download dependencies,
or execute hooks.

## Output and reproducibility

```text
Output.app/Contents/
  Info.plist
  Executable/app
  Resources/icon.svg (or icon.png)
  Resources/LICENSE
```

The manifest's `License` dictionary stores `Identifier` and `File`; `File` is
relative to Resources. Existing launchers ignore that additional dictionary.
Executable permissions are 0700, data files 0600, and directories 0700.
The source is not changed. Identical recipe values and input bytes produce
identical output bytes and permission modes, regardless of output name.
Timestamps, owner IDs, filesystem metadata, and upstream compiler output are
not normalized; no byte-reproducible archive or reproducible compilation is
claimed. The output is an unsigned directory, not a distributable archive.

Version 1 deliberately supports one executable, one icon, and one licence.
Embed additional assets at build time (as the sample does), or wait for a
separately tested resource/library mapping extension. Required Qt/system
libraries must already exist on the target. There is no automatic dependency
bundling, library-path rewriting, sandbox, signing, update, or macOS runtime.
Provenance is descriptive metadata, not cryptographic proof.

## Verification and merge gate

```sh
cmake --build build --target test-applicationbundlepackager
ctest --test-dir build -R '^northstar-applicationbundlepackager$' --output-on-failure
QT_QPA_PLATFORM=offscreen build-packaging-demo/northstar-packaging-demo --self-test
```

The main build also builds the sample when `BUILD_TESTING` is enabled. Its
`northstar-packaging-demo` CTest exercises the actual CLI and the graphical
sample with an isolated XDG data directory and offscreen Qt backend.

The focused test covers deterministic bytes, XML escaping, inspection,
user-scoped installation, launch, duplicate refusal, Trash removal, schema
errors, unsafe paths/permissions, symlinks, FIFO rejection, oversized input,
and invalid icon/licence content. Required native CI must pass. Physical
acceptance for this slice is the graphical sample install, launch, and removal
described above; completed hardware tests are not repeated.

### Initial implementation evidence

On the physical Intel FreeBSD 15.1 amd64 host, the packaging tool and sample
built with the installed Qt 6.11.1 toolchain. Both focused CTest entries passed;
the packager QtTest reported 17 passes, zero failures. Repository contracts and
`git diff --check` passed on the development PC. The user also confirmed manual
Files installation, Applications launch, and Trash-backed removal of the
graphical sample on the Intel laptop. The installed shell is unchanged.
