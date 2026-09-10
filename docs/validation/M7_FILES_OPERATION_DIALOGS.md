# Files operation dialog extraction

Five existing dialogs are moved into dedicated QML components: naming (file,
folder and rename), paste conflicts, move to Trash, restore, and empty Trash.
Owner window and theme are explicit required inputs. The naming field exposes
an alias for the existing initialization flow. No controller implementation,
wording, sizing, acceptance/retry logic, authorization or storage scope changes.

The shell graphical self-test opens, rejects and reopens each component without
accepting a filesystem mutation. Existing controller tests cover file operations;
surface contracts explicitly check the extracted controller wiring.

## Physical acceptance pending

Use only disposable test files in Home: create a folder/file, rename, cancel and
reopen a naming dialog; copy into a conflicting destination and choose Keep Both;
delete the test item and restore it from Trash. Check the Empty Trash dialog by
cancelling it, without deleting unrelated Trash contents. Dialog appearance,
placement and existing operation behavior should remain unchanged.

Automated build/test results are recorded in the PR. This remains a refactor,
not new USB support or completion of the entire Files decomposition.
