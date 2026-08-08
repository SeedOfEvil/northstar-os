# M3 keyboard and menu command slice

Northstar now keeps the first application-facing keyboard mappings in a
project-owned `ShortcutCatalog`. The catalog is the single source of truth
for both the system-menu accelerator hints and the shell's application
shortcuts:

| Command | Shortcut |
| --- | --- |
| Applications | `Meta+K` |
| Software Center | `Meta+U` |
| Files | `Meta+E` |
| Settings | `Meta+,` |
| Terminal | `Ctrl+Alt+T` |
| Firefox | `Meta+B` |
| Refresh Applications | `Meta+R` |

`Escape` dismisses the active Northstar transient surface. The mappings are
application-level shortcuts owned by the shell; compositor-wide global
shortcuts and universal third-party toolkit menus remain outside this slice.

## Validation

From the FreeBSD development checkout:

```sh
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

Open the shell and confirm that the accelerators appear in the Northstar menu,
that each shortcut opens its corresponding surface or application, and that
`Escape` closes the currently active menu, overview, Software, Files, Settings,
or Quick Settings surface.
