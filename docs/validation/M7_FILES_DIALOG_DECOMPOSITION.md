# Files dialog decomposition: first slice

The application-install window is extracted from `FileBrowserWindow.qml` into
`FilesBundleInstallDialog.qml`. The owner window and palette are explicit required
inputs. The status text is exposed as an alias so the existing open/reset workflow
does not depend on a private child id.

No layout, labels, controller calls, authorization rules or installation behavior
are intentionally changed. The original transient parent, sizing, placement,
success/duplicate handling and scrolling are preserved.

Surface contracts follow the extracted component. The shell QML self-test retains
invalid-bundle and long-content coverage and now checks that reopening clears the
previous operation's success/error/status state.

## Acceptance

Repository and QML surface contracts pass. Native build and graphical self-test
results are recorded in the PR. Physical acceptance remains pending: open a bundle
from Files, verify the familiar dialog, cancel/reopen, and confirm installed-bundle
and compatibility messages remain unchanged. Check moving/resizing Files and
dialog placement. No USB/storage changes or image rebuild belong to this slice.

This is the first bounded extraction, not completion of the full Files refactor.
Other dialogs and the larger browsing surface remain subsequent slices.
