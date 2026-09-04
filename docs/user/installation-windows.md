# Windows Installation Notes

This guide covers practical setup for building and running Perastage on Windows. It is intentionally concise and focused on the recommended Windows workflow.

For the complete build and dependency reference, see [Build and Dependency Guide](../developer/build.md).

## Recommended Toolchain

- Visual Studio 2022 with the C++ desktop workload.
- CMake, either bundled with Visual Studio or installed separately.
- Ninja, either bundled with Visual Studio or installed separately.
- A classic vcpkg installation at the selected classic vcpkg checkout with dependencies installed under `$env:VCPKG_ROOT\installed\x64-windows`.

## Select a vcpkg checkout

Set `VCPKG_ROOT` to the existing classic vcpkg checkout before using Visual Studio or CMake. The explicit `-VcpkgRoot` setup-script parameter takes precedence over an existing environment value, and the script exports its validated selection for the preset. The shared Windows Ninja presets use their top-level `toolchainFile` field to select `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`, disable vcpkg manifest mode and manifest auto-install, and reuse `$env{VCPKG_ROOT}/installed/x64-windows`. Opening the repository in Visual Studio should not run vcpkg install, build packages, or create a `vcpkg_installed` directory in the repository or build tree.

For direct Visual Studio use, either define `VCPKG_ROOT` persistently in the Windows user environment and restart Visual Studio, or define it in the `environment` map of an ignored `CMakeUserPresets.json` that inherits a canonical Perastage Windows preset. Select that shared or inherited preset after opening the folder.

CI remains separate: GitHub Actions still uses the repository manifest, isolated installed roots, caches, and the pinned baseline for reproducible release builds.

## Install dependencies

Select the classic checkout and install Windows dependencies once before configuring:

```powershell
$env:VCPKG_ROOT = 'D:\path\to\vcpkg'
```

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows wxwidgets[secretstore] gettext[tools] tinyxml2 curl glew zlib nanovg podofo meshoptimizer backward-cpp mdns
```

The manifest requests `wxwidgets[secretstore]` for Windows Credential Manager support and declares Windows gettext tools as a host dependency for CI and dependency documentation. For local Windows builds, `setup_windows.ps1` validates that the installed tree is ready; it does not install or rebuild packages.

If wxWidgets was previously built without secure-store support, repair that package in the selected classic vcpkg checkout, then use `-CleanBuild` so CMake probes the rebuilt classic dependency instead of a stale build cache.

## Configure and Build with Visual Studio

Open the repository folder in Visual Studio.

Select:

```text
Local Machine
```

Then select a Windows configure preset:

```text
Windows x64 Debug (Ninja)
```

And select the matching build preset:

```text
Build Windows Debug (Ninja)
```

For release builds, use:

```text
Windows x64 Release (Ninja)
Build Windows Release (Ninja)
```

## Configure and Build from PowerShell

From a Visual Studio Developer PowerShell in the repository root, validate dependencies and configure a Debug build:

```powershell
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild
cmake --preset win-x64-debug-ninja
```

Build it:

```powershell
cmake --build --preset win-debug-build-ninja
```

For a Release build:

```powershell
.\setup_windows.ps1 -Configuration Release -CleanBuild -SkipBuild
cmake --preset win-x64-release-ninja
cmake --build --preset win-release-build-ninja
```

## Run

- Launch `Perastage.exe` from the generated build output.
- Use `perastage_stage` when preparing a packaging-ready runtime folder.

## Installed binaries vs editable user library

Perastage keeps executable binaries in the installation directory, for example:

```text
C:\Program Files\Perastage
```

when installed by Inno Setup.

Editable library data is stored in the per-user data directory.

### Editable library location (Windows)

- Default writable root: `%APPDATA%\Perastage\library\`
- Typical subfolders used by the app:
  - `%APPDATA%\Perastage\library\fixtures\`
  - `%APPDATA%\Perastage\library\trusses\`
  - `%APPDATA%\Perastage\library\scene_objects\`
  - `%APPDATA%\Perastage\library\misc\`
  - `%APPDATA%\Perastage\library\projects\`
  - `%APPDATA%\Perastage\library\default_layouts\`

Support note: users can open this location directly from **Tools -> Open user library folder** in the application.

### Bootstrap / migration behavior

At startup, Perastage runs a non-destructive bootstrap migration:

1. It reads bundled seed library content from the installed app directory.
2. It copies missing files into `%APPDATA%\Perastage\library\`.
3. It never overwrites existing user files during this bootstrap.

This lets installers update bundled defaults while preserving user-edited library content.

### Permissions expectation

Editing library content does not require administrator privileges because writes happen in the user profile tree, not inside `Program Files`.

## If Configuration Fails

- Run `setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild` from the repository root.
- Verify that `$env{VCPKG_ROOT}/vcpkg.exe`, `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`, and `$env{VCPKG_ROOT}/installed/x64-windows` exist, or pass `-VcpkgRoot` to validate another existing classic vcpkg installation.
- Remove stale build directories and reconfigure.
- If Visual Studio keeps stale state, close Visual Studio and remove the local `.vs` folder.
- Follow the detailed fix paths in [Troubleshooting](troubleshooting.md).

## Checking Windows Credential Manager support

After installing or rebuilding dependencies, save valid GDTF Share credentials, restart Perastage, and open the GDTF download workflow. A normal Windows build should not show a secure-storage unavailable warning, and the credentials should not be requested again solely because the application restarted. You can also verify that Windows Credential Manager contains an entry created by Perastage for the saved test credentials.

## Manual Credential Manager validation

A successful CMake secure-store probe confirms that wxWidgets was compiled with `wxUSE_SECRETSTORE`, but release validation should also exercise Windows Credential Manager at runtime. Save valid GDTF Share credentials, restart Perastage, open GDTF download, confirm the online catalog loads without asking for credentials again, download one GDTF, verify the Perastage Credential Manager entry exists, clear credentials from Perastage, and confirm the native entry is removed. Repeat once with a password containing a double quote, a backslash, and Unicode text.
