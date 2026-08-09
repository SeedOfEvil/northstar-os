# Northstar applications

First-party applications will live here: filer, settings, software, and terminal. They use the same project design tokens and service interfaces as the shell while remaining ordinary FreeBSD-packaged applications.

`samples/NorthstarWelcome.app` is the first development bundle. It demonstrates
the project-owned `Contents/Info.plist`, `Contents/Executable`, and
`Contents/Resources` layout. The launcher validates it before exposing it in
Apps and Open With; release bundles still require FreeBSD package ownership
and provenance integration.

When a graphical session is available, the bundle's executable opens the
native `northstar-welcome-gui` surface with Home, Desktop, and Explore actions.
In a recovery or terminal-only session it retains the qterminal/xterm text
fallback, so the bundle remains useful without pretending that a GUI exists.
The native target supports `--self-test` for headless CI and VM validation.
