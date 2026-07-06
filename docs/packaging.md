# Packaging and Platform Integration

This document describes the supported distribution and file-association behavior for Perastage.

## Packaging source of truth

Perastage currently has two different build contexts:

- **Local developer builds** use the CMake presets documented in [Build and dependency guide](build.md).
- **GitHub Actions release builds** use dedicated workflow environments and may use their own dependency cache paths.

Do not assume that local dependency paths, such as `C:/vcpkg`, also apply to CI workflows. CI packaging workflows define their own paths inside the GitHub Actions workspace.

## Windows Packaging

The official Windows installer workflow uses Inno Setup:

- Workflow: `.github/workflows/windows-installer.yml`
- Script: `packaging/windows/Perastage.iss`
- Build Release and run `perastage_stage` before compiling the installer.
- Optional associations can include `.pstg` and `.mvr`.
- Installer binaries target `DefaultDirName={autopf}\Perastage` (standard Program Files install).

### Windows CI packaging flow

The Windows installer workflow is the authoritative CI packaging flow. It performs these high-level steps:

1. Check out the requested source ref.
2. Bootstrap a CI-local vcpkg checkout inside the GitHub Actions workspace.
3. Install the required Windows dependencies into the CI vcpkg cache.
4. Configure and build Perastage in Release mode.
5. Run the `perastage_stage` target.
6. Collect debug symbols.
7. Build the Inno Setup installer.
8. Upload the staged application, installer, and symbols as workflow artifacts.

This CI flow intentionally uses workspace-local vcpkg paths and cache directories. That is separate from the recommended local Windows development setup, which uses the CMake presets described in [Build and dependency guide](build.md).

### Local Windows installer flow

For local packaging work, first make sure the normal Windows Release build works from the repository root.

Recommended local staging flow:

```powershell
cmake --preset win-x64-release
cmake --build --preset win-release-build
cmake --build --preset win-release-stage
```

If using the Ninja Release preset instead:

```powershell
cmake --preset win-x64-release-ninja
cmake --build --preset win-release-build-ninja
cmake --build --preset win-release-stage-ninja
```

Then compile the Inno Setup script:

```powershell
$version = (cmake -P packaging/windows/get_project_version.cmake |
  Select-String -Pattern '-- ([0-9]+\.[0-9]+\.[0-9]+)$').Matches[0].Groups[1].Value

iscc /DMyAppVersion=$version packaging/windows/Perastage.iss
```

The generated installer is written to:

- `out/installer/Perastage_<version>_Setup.exe`

### Writable data model for support

Windows packaging follows a split model:

- **Installed tree (`{autopf}\Perastage`)**: binaries and bundled base assets.
- **User-data tree (`%APPDATA%\Perastage\library`)**: editable fixtures, trusses, scene objects, dictionaries, and related user library content.

Perastage bootstrap/migration behavior on startup is non-destructive:

1. Seed content from the installed library is used as source.
2. Missing files are copied into the user library.
3. Existing files in user-data are kept (no overwrite).

Operationally, this means support can tell users:

- "You do not need admin rights to edit your library."
- "Use **Tools -> Open user library folder** to jump to the editable location."

### Version source of truth

Perastage version is defined in one place:

- `VERSION` at the repository root.

When you want a new global version for generated artifacts, update that file only. The root CMake configuration reads it into `project(Perastage VERSION ...)`, and the helper script `packaging/windows/get_project_version.cmake` passes it to Inno Setup (`/DMyAppVersion=...`).

### Enabling `.mvr` association in the installer

The Inno Setup script already includes an optional task named `assoc_mvr`.

- In interactive installer mode, enable the checkbox:
  - **Associate .mvr files with Perastage (optional import workflow)**
- In silent/automated installs, include task selection:
  - `/TASKS="assoc_pstg,assoc_mvr"` (or only `assoc_mvr` if preferred)

When selected, the installer registers:

- ProgID: `Perastage.MVR`
- Open command: `"Perastage.exe" "%1"`
- Extension mapping: `.mvr -> Perastage.MVR`

The uninstall behavior is conservative and removes Perastage-owned registration values without aggressively deleting global extension ownership.

### Association notes

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

## Release debug symbols

Release workflows preserve matching debug symbols as separate ZIP assets for maintainers:

- Windows: `Perastage-<version>-Windows-symbols.zip` contains collected `.pdb` files.
- Linux AppImage: `Perastage-<version>-Linux-symbols.zip` contains separated debug data for the staged executable.
- macOS: `Perastage-<version>-macOS-symbols.zip` contains the generated `.dSYM` bundle.
- Arch Linux: `Perastage-<version>-ArchLinux-symbols.zip` contains the generated debug package when makepkg produces one.

Users do not need these files to run Perastage. They are uploaded with release assets so maintainers can match a local crash report to the exact binary build that shipped.

## Related Documents

- [Build and dependency guide](build.md)
- [Storage policy (installation vs user profile)](storage_policy.md)
- [Troubleshooting](troubleshooting.md)
