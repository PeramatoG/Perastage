# Troubleshooting

## MVR open and import problems

Confirm the extension is `.mvr`, try another file to isolate the problem, and review import messages in the Console. Use the [Inspector](inspector.md) to examine an external package without importing it. For merge conflicts, see [Opening MVR Files](opening-mvr-files.md).

## Missing or incorrect GDTF fixtures

Check the fixture definition and selected DMX mode. Use **Tools → Download GDTF** for profiles and **Tools → Edit dictionaries** for local mappings. Reopen and review the scene after correcting missing profiles. Check the [definition completion policy](preferences.md#gdtf-definition-completion) if generated views differ from your expectations.

## User library

Use **Tools → Open user library folder**; on Windows the default is `%APPDATA%\Perastage\library\`. Confirm your user profile is writable and back up custom content before replacing files. Edit user-library files rather than installed defaults.

## Layout and PDF problems

Select a non-empty layout, check its frames and orientation, and verify scene/table content before **File → Print Layout...**. Choose a writable destination and a non-empty filename. Review the exported PDF; see [Layouts and PDF](layouts.md) for the workflow.

## Project and runtime problems

Save a separate project copy when possible, restart Perastage and reopen it. Recheck critical data in 2D, 3D and tables. For slow 3D navigation, lower [Model detail](preferences.md#3d-viewer) or enable the navigation proxy. Compare with a small project to isolate scene-specific problems.

## GDTF Share credentials

Online loads and downloads require an account and network access; [cached catalog browsing](gdtf-download.md#cached-and-offline-browsing) remains available offline.

If Perastage warns that the runtime secure credential store is unavailable, the password is usable for the current operation but is not retained. Check that your operating-system credential store is available; Linux needs an active Secret Service provider such as GNOME Keyring or KWallet. If the warning persists in a release package, include it in your report. Do not include passwords.

## Logs and diagnostic reports

Use **Help → Open Logs Folder** to find `perastage.log`, the previous launch's `perastage.previous.log`, and the `crash_reports` subfolder.

Default log locations:

- Windows: `%LOCALAPPDATA%\Perastage\logs\`
- macOS: `~/Library/Logs/Perastage/`
- Linux: `${XDG_STATE_HOME}/perastage/logs/`, or `~/.local/state/perastage/logs/` if unset.

Use **Help → Export Diagnostic Report...** for a text report with build/platform details, available OpenGL information and recent log lines. Reports stay local and are not uploaded automatically. Review the report before sharing it, together with reproduction steps and a small example file when possible.

Source-build, dependency and toolchain failures belong in the [developer build guide](../developer/build.md).
