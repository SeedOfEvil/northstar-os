# Northstar applications

First-party applications will live here: filer, settings, software, and terminal. They use the same project design tokens and service interfaces as the shell while remaining ordinary FreeBSD-packaged applications.

`samples/NorthstarWelcome.app` is the first development bundle. It demonstrates
the project-owned `Contents/Info.plist`, `Contents/Executable`, and
`Contents/Resources` layout. The launcher validates it before exposing it in
Apps and Open With; release bundles still require FreeBSD package ownership
and provenance integration.

`NorthstarTextEditor.app` is the first editable first-party app. It accepts a
text-file path from Files/Open With, loads UTF-8 documents up to 8 MiB, and
saves changes through an atomic user-owned write. It is deliberately a small
text editor, not a general document suite; unsupported or oversized files are
rejected with a visible status message.
