# Northstar applications

First-party applications will live here: filer, settings, software, and terminal. They use the same project design tokens and service interfaces as the shell while remaining ordinary FreeBSD-packaged applications.

`samples/NorthstarWelcome.app` is the first development bundle. It demonstrates
the project-owned `Contents/Info.plist`, `Contents/Executable`, and
`Contents/Resources` layout. The launcher validates it before exposing it in
Apps and Open With; release bundles still require FreeBSD package ownership
and provenance integration.
