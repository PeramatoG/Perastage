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
$version = (cmake -P packaging/windows/get_project_version.cmake |
  Select-String -Pattern '-- ([0-9]+\.[0-9]+\.[0-9]+)$').Matches[0].Groups[1].Value
iscc /DMyAppVersion=$version packaging/windows/Perastage.iss
```

The generated installer is written to:

- `out/installer/Perastage_<version>_Setup.exe`

### Version Source of Truth

Perastage version is defined in one place:

- `CMakeLists.txt` in the root `project(Perastage VERSION X.Y.Z ...)` declaration.

When you want a new global version for generated artifacts, update that line only.
The helper script `packaging/windows/get_project_version.cmake` reads that value and
passes it to Inno Setup (`/DMyAppVersion=...`).

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

## Linux Packaging

The official generic Linux release asset remains the AppImage produced by `.github/workflows/linux-installer.yml`. This is the recommended download for general Linux distribution because it keeps the existing staged application layout and bundles the runtime pieces expected by the AppImage flow.

Perastage also generates an experimental Arch Linux pacman package from `.github/workflows/arch-package.yml`:

- Artifact name: `Perastage-<version>-arch-x86_64.pkg.tar.zst`
- Packaging recipe: `packaging/arch/PKGBUILD`
- Build environment: `archlinux:base-devel` in GitHub Actions
- Intended audience: Arch-based distributions such as Arch Linux, Manjaro, and EndeavourOS

The Arch package uses the project CMake install target, keeps runtime assets under `/opt/perastage` so resource lookup matches the current Linux application layout, and exposes a `/usr/bin/Perastage` launcher symlink. Desktop metadata, MIME metadata, and the application icon are installed into standard `/usr/share` locations.

This Arch package is experimental until it receives broader runtime testing on real Arch-based desktop systems. If a user only needs the most portable Linux release asset, use the AppImage.

## Linux Desktop Integration

Install layouts include desktop and MIME metadata:

- `share/applications/perastage.desktop`
- `share/mime/packages/perastage-mime.xml`
- `share/icons/hicolor/1024x1024/apps/perastage.png`

During `cmake --install`, cache refresh commands may run to expose new associations.

## macOS Document Association

Perastage app bundles declare `.mvr` document type metadata through bundle settings so Finder can route files to the application.

## macOS DMG Packaging (Unsigned Builds)

Perastage macOS CI currently produces an unsigned/not-notarized `.app` and `.dmg` because there is no Apple Developer ID certificate configured for this project.

### What CI validates

The macOS installer workflow validates that:

- `Perastage.app` is staged with expected bundle layout (`Contents/Info.plist`, executable, and `.icns` files).
- The executable keeps its executable bit.
- `Info.plist` passes `plutil -lint`.
- The app is ad-hoc signed (`codesign --sign -`) and verified for internal signature consistency.
- Quarantine metadata is cleared from the staged app and collected DMG in CI.
- The generated DMG mounts successfully and contains a launchable `Perastage.app` bundle.

### Expected first-run behavior on end-user macOS systems

When users download builds from the internet, macOS can add quarantine metadata again, even if CI removed it during packaging.

Because the app is unsigned/not notarized, users may see messages such as:

- `"Perastage" is damaged and can't be opened.`
- `"Perastage" cannot be opened because the developer cannot be verified.`

This is expected for non-notarized internet downloads. The long-term production fix is Developer ID signing + notarization.

### User workaround for quarantine blocking

If the app is copied to `/Applications`:

```bash
xattr -dr com.apple.quarantine /Applications/Perastage.app
```

If running from Downloads:

```bash
xattr -dr com.apple.quarantine ~/Downloads/Perastage.app
```

GUI alternative:

1. Open **System Settings**.
2. Go to **Privacy & Security**.
3. Use the **Allow/Open anyway** option shown for the blocked app.

### Distribution note

GitHub Actions artifacts are downloaded as ZIP files, which may preserve or add quarantine metadata. For public releases, prefer attaching the `.dmg` directly as a GitHub Release asset.

## Related Documents

- [Build and dependency guide](build.md)
- [Storage policy (installation vs user profile)](storage_policy.md)
- [Troubleshooting](troubleshooting.md)
