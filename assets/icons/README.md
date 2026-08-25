# Northstar icon set

`northstar-icons.png` is the processed Northstar OS icon sheet generated for
the shell surfaces. It uses a transparent 4-by-3 layout with 362-pixel tiles:

| Tile | Icon |
| --- | --- |
| 0 | Files / folder |
| 1 | Terminal |
| 2 | Northstar browser |
| 3 | Settings |
| 4 | Applications |
| 5 | Quick settings |
| 6 | Trash |
| 7 | Search |
| 8 | Desktop |
| 9 | Text editor / file |
| 10 | Information |
| 11 | Northstar mark |

The source render is retained as `northstar-icons-source.png` for provenance;
the shell consumes only the alpha-processed PNG.

## Aurora Glass atlas

`northstar-icons-aurora.png` is the generated, alpha-backed replacement used
by the Aurora Glass shell. It retains the same semantic 4-by-3 tile order while
reducing gloss, bevels, and heavy outlines. Its unprocessed generation is kept
as `northstar-icons-aurora-source.png`; the original Lunar atlas remains in the
repository for provenance and rollback.

`northstar-system-icons-aurora.png` is the matching 5-by-4 atlas for every
Settings category, top-bar status item, Quick Settings control, and utility
role visible in the approved Aurora reference; its unprocessed generation is
retained as `northstar-system-icons-aurora-source.png`.
`northstar-activity-aurora.png`
is its eight-frame transparent activity strip for discovery, pairing, loading,
and transaction states; static controls do not animate without meaning.

## Generated application and control icons

The project-owned `generated/` directory contains four derived raster icons
for surfaces that need a richer identity than the original sheet:

| File | Surface |
| --- | --- |
| `northstar-welcome.png` | Northstar Welcome |
| `northstar-software.png` | Software Center |
| `northstar-notifications.png` | Notification Center |
| `northstar-power.png` | Logout, restart, and shutdown |

Each processed PNG has a matching `*-source.png` chroma-key render retained
for provenance. The source renders are not installed; the shell installs only
the validated alpha assets.
