# Packaging and Platform Integration

This document describes the supported distribution and file-association behavior for Perastage.

## Windows Packaging

The official Windows installer workflow uses Inno Setup:

- Script: `packaging/windows/Perastage.iss`
- Build Release and run `perastage_stage` first.
- Compile the installer script in Inno Setup.
- Optional associations can include `.pstg` and `.mvr`.
- Installer binaries target `DefaultDirName={autopf}\Perastage` (standard Program Files install).

### Writable data model for support

Windows packaging follows a split model:

- **Installed tree (`{autopf}\Perastage`)**: binaries and bundled base assets.
- **User-data tree (`%APPDATA%\Perastage\library`)**: editable fixtures, trusses, scene objects, dictionaries, and related user library content.

Perastage bootstrap/migration behavior on startup is non-destructive:

1. Seed content from the installed library is used as source.
2. Missing files are copied into the user library.
3. Existing files in user-data are kept (no overwrite).

Operationally, this means support can tell users:

- “You do not need admin rights to edit your library.”
- “Use **Tools → Open user library folder** to jump to the editable location.”

### Recommended End-to-End Windows Installer Flow

From a Developer PowerShell at repository root:

```powershell
cmake -S . -B out/build/x64-Release -G "Visual Studio 17 2022" -A x64
cmake --build out/build/x64-Release --config Release
cmake --build out/build/x64-Release --config Release --target perastage_stage
```

Then compile the Inno Setup script:

```powershell
iscc packaging/windows/Perastage.iss
```

The generated installer is written to:

- `out/installer/Perastage_<version>_Setup.exe`

### Enabling `.mvr` Association in the Installer

The Inno Setup script already includes an optional task named `assoc_mvr`.

- In interactive installer mode, enable the checkbox:
  - **Associate .mvr files with Perastage (optional import workflow)**
- In silent/automated installs, include task selection:
  - `/TASKS="assoc_pstg,assoc_mvr"` (or only `assoc_mvr` if preferred)

When selected, the installer registers:

- ProgID: `Perastage.MVR`
- Open command: `"Perastage.exe" "%1"`
- Extension mapping: `.mvr -> Perastage.MVR`

The uninstall behavior is conservative and removes Perastage-owned registration
values without aggressively deleting global extension ownership.

### Association Notes

- Installer writes association entries under `Software\Classes`.
- Uninstall behavior is conservative and avoids aggressive global extension ownership cleanup.
- Legacy CPack/NSIS wiring exists for compatibility but is not the primary path.

## Linux Desktop Integration

Install layouts include desktop and MIME metadata:

- `share/applications/perastage.desktop`
- `share/mime/packages/perastage-mime.xml`
- `share/icons/hicolor/1024x1024/apps/perastage.png`

During `cmake --install`, cache refresh commands may run to expose new associations.

## macOS Document Association

Perastage app bundles declare `.mvr` document type metadata through bundle settings so Finder can route files to the application.

## Related Documents

- [Build and dependency guide](build.md)
- [Storage policy (installation vs user profile)](storage_policy.md)
- [Troubleshooting](troubleshooting.md)
