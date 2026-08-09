# Northstar applications

First-party applications will live here: filer, settings, software, and terminal. They use the same project design tokens and service interfaces as the shell while remaining ordinary FreeBSD-packaged applications.

`samples/NorthstarWelcome.app` is the first development bundle. It demonstrates
the project-owned `Contents/Info.plist`, `Contents/Executable`, and
`Contents/Resources` layout. The launcher validates it before exposing it in
Apps and Open With; release bundles still require FreeBSD package ownership
and provenance integration.

When a graphical session is available, the bundle's executable opens the
native `northstar-welcome-gui` surface with real Home/Desktop folder actions,
an explicit informational desktop guide, a Getting Started checklist, and
version/platform/session status. In a recovery or terminal-only session it
retains the qterminal/xterm text fallback, so the bundle remains useful
without pretending that a GUI exists. The guide points to shell-owned Apps,
Settings, and Software surfaces rather than claiming the standalone Welcome
process can control them directly.
The native target supports `--self-test` for headless CI and VM validation.

`NorthstarTextEditor.app` is the first editable first-party app. It accepts a
text-file path from Files/Open With, loads UTF-8 documents up to 8 MiB, and
saves changes through an atomic user-owned write. It is deliberately a small
text editor, not a general document suite; unsupported or oversized files are
rejected with a visible status message.
