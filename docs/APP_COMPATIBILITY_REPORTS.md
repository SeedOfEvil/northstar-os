# Application compatibility reports

`northstar-app report <bundle.app-or-file>` reads format evidence without executing
the application, its interpreter, a dependency loader, or any network request.
The Files installation dialog and Applications information display the same report
for Northstar bundles. Standalone binaries can be reported from the command line;
this does not add a standalone-binary installer.

Reports distinguish web applications, unverified native/script formats, unsupported
PE/Mach-O formats, and invalid Northstar bundles. All reports explicitly retain
`RuntimeVerified=no`. A successful report command means a report was produced,
not that the application can run. Invalid bundles provide a repair direction.
Existing ownership, tree, duplicate and installation checks remain authoritative.

ELF class, ABI marker and machine number are header evidence only. In particular,
System V/unspecified ABI does not mean FreeBSD, and a matching header does not prove
libraries or runtime compatibility. Scripts are not executed to test interpreters.
Web reports explain Firefox, internet and shared browser storage requirements;
they do not probe the browser or website. Reports do not verify signatures or safety.

Header reads are bounded, reject final symlinks and nonregular files on Unix, and
never launch `ldd`. PE signature lookup is capped at 1 MiB. Unknown/container formats,
including universal Apple binaries, stay unverified rather than guessed from suffixes.

Format references: [ELF header](https://gabi.xinuos.com/elf/02-eheader.html),
[PE signature](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

## Focused acceptance (pending)

Automated checks on the Intel laptop (FreeBSD 15.1) passed: bundle catalog,
compatibility header fixtures, packaging lifecycle and web CLI tests (4/4),
plus the offscreen shell QML self-test including long report text. Repository
contracts and whitespace checks also passed. These are not physical UI acceptance.

- Open a native bundle in Files: report says runtime/dependencies unverified; install still works.
- Open a web bundle: Firefox/internet/shared-profile disclosure remains readable.
- Open Applications information for either installed bundle: report is readable.
- Report a standalone PE/Mach-O fixture and invalid bundle: clear explanation, no execution.
- Existing install success/duplicate/remove behavior remains unchanged.

No image rebuild or unrelated hardware regression is required for this slice.
Physical acceptance remains the merge gate.
