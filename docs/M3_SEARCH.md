# M3 unified search

Northstar Search is the keyboard-first launcher opened from the top bar or with
`Ctrl+K`. It replaces the earlier one-line routing field with a categorized
overlay backed by `SearchController`.

## Result contract

Every result is a bounded record with `kind`, `id`, `title`, `subtitle`,
`icon`, `iconSource`, `category`, and validated `activationData`. The supported
kinds are:

- `action`: one of Applications, Files, Settings, Software Center, Terminal,
  or Firefox;
- `application`: a current entry from the validated desktop or Northstar app
  bundle catalog;
- `file` or `folder`: a canonical existing path below the current user's Home.

The controller revalidates action IDs, application desktop IDs, and canonical
Home containment at activation time. Search never interprets a result as a
shell command and this slice does not send a query to the web.

## Responsiveness and bounds

Action and application matches are produced immediately on the UI thread.
File search starts after a 140 ms debounce and runs through Qt Concurrent so
directory traversal cannot block typing. A changed query cancels the previous
worker result by generation and cancellation token.

The scanner:

- ignores hidden entries and symbolic links;
- never follows a path outside canonical Home;
- examines at most 20,000 entries;
- returns at most 24 file/folder results;
- combines those with at most 6 actions and 10 applications.

## Interaction acceptance

At 1280x800, validate that:

1. Clicking the top search field and pressing `Ctrl+K` both open the overlay
   with keyboard focus in its text field.
2. Typing remains responsive while Home contains a representative directory
   tree and the footer reports background file search.
3. Up and Down change selection, Enter activates it, and Escape dismisses the
   overlay without activating anything.
4. Settings, Software Center, Terminal, Firefox, Files, and Applications route
   to their existing safe surfaces.
5. A catalog application launches by desktop ID, a folder opens in Files, and
   a file follows the existing association/Open With flow.
6. Hidden files, paths outside Home, arbitrary commands, and web queries do not
   appear as executable search results.

Automated coverage lives in `northstar-searchcontroller` and the QML surface
contract. The scfb/pixman VM remains the manual interaction lane; this search
slice makes no DRM/KMS or animation-quality claim.
