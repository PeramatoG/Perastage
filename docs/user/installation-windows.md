# Windows Installation Notes

This guide covers practical setup for building and running Perastage on Windows. It is intentionally concise and focused on the recommended Windows workflow.

For the complete build and dependency reference, see [Build and Dependency Guide](../developer/build.md).

## Recommended Toolchain

- Visual Studio 2022 with the C++ desktop workload.
- CMake, either bundled with Visual Studio or installed separately.
- Ninja, either bundled with Visual Studio or installed separately.
- The Perastage-local vcpkg checkout created by `setup_windows.ps1` in `.tools/vcpkg`.

## Recommended vcpkg location

The shared Windows presets keep `C:/vcpkg` as a stable fallback and allow vcpkg manifest mode so Visual Studio can install missing dependencies into the normal `C:/vcpkg/installed` tree when setup has not been run. The setup script creates `.tools/vcpkg` by default, installs dependencies into `vcpkg_installed`, and writes an ignored `CMakeUserPresets.json` with manifest mode disabled so Visual Studio can use those prepared local paths. If you intentionally use another vcpkg checkout, pass `-VcpkgRoot` to the setup script or create a local `CMakeUserPresets.json`. See [Build and Dependency Guide](../developer/build.md) for details.

## Install dependencies

Install Windows dependencies through the root vcpkg manifest by running the setup script:

```powershell
cd C:\path\to\Perastage
.\setup_windows.ps1 -Configuration Release -CleanBuild
```

The script pins and bootstraps a Perastage-specific vcpkg checkout, installs all manifest dependencies into `vcpkg_installed`, writes local CMake presets for that same tree, and configures CMake against it. The manifest requests `wxwidgets[secretstore]` for Windows Credential Manager support and declares Windows gettext tools as a host dependency for localization catalog generation. Do not run a separate gettext install command from the repository root.

If wxWidgets was previously built without secure-store support, use `-CleanBuild` so CMake probes the rebuilt manifest dependencies instead of a stale build cache.

## Configure and Build with Visual Studio

Open the repository folder in Visual Studio.

Select:

```text
Local Machine
```

Then select a Windows configure preset, for example:

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

From a Visual Studio Developer PowerShell in the repository root, prepare dependencies and configure a Debug build:

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
- Verify that `.tools/vcpkg/vcpkg.exe` and `vcpkg_installed/x64-windows` exist, or verify the equivalent paths from your explicit `-VcpkgRoot` override.
- Remove stale build directories and reconfigure.
- If Visual Studio keeps stale state, close Visual Studio and remove the local `.vs` folder.
- Follow the detailed fix paths in [Troubleshooting](troubleshooting.md).

## Checking Windows Credential Manager support

After installing or rebuilding dependencies, save valid GDTF Share credentials, restart Perastage, and open the GDTF download workflow. A normal Windows build should not show a secure-storage unavailable warning, and the credentials should not be requested again solely because the application restarted. You can also verify that Windows Credential Manager contains an entry created by Perastage for the saved test credentials.

## Manual Credential Manager validation

A successful CMake secure-store probe confirms that wxWidgets was compiled with `wxUSE_SECRETSTORE`, but release validation should also exercise Windows Credential Manager at runtime. Save valid GDTF Share credentials, restart Perastage, open GDTF download, confirm the online catalog loads without asking for credentials again, download one GDTF, verify the Perastage Credential Manager entry exists, clear credentials from Perastage, and confirm the native entry is removed. Repeat once with a password containing a double quote, a backslash, and Unicode text.
