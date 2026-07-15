# Windows Installation Notes

This guide covers practical setup for building and running Perastage on Windows. It is intentionally concise and focused on the recommended Windows workflow.

For the complete build and dependency reference, see [Build and Dependency Guide](../developer/build.md).

## Recommended Toolchain

- Visual Studio 2022 with the C++ desktop workload.
- CMake, either bundled with Visual Studio or installed separately.
- Ninja, either bundled with Visual Studio or installed separately.
- vcpkg installed in `C:/vcpkg`.

## Recommended vcpkg location

On Windows, the recommended Perastage vcpkg location is:

```text
C:/vcpkg
```

The shared Windows presets in `CMakePresets.json` use this path directly.

This avoids conflicts with Visual Studio Developer PowerShell, which may set `VCPKG_ROOT` to Visual Studio's internal vcpkg directory.

If your vcpkg installation is in another location, either edit the Windows `CMAKE_TOOLCHAIN_FILE` values in `CMakePresets.json` or create a local `CMakeUserPresets.json`. See [Build and Dependency Guide](../developer/build.md) for details.

## Install dependencies

Install the required Windows dependencies with:

```powershell
C:\vcpkg\vcpkg.exe install wxwidgets:x64-windows tinyxml2:x64-windows curl:x64-windows glew:x64-windows meshoptimizer:x64-windows nanovg:x64-windows podofo:x64-windows zlib:x64-windows backward-cpp:x64-windows mdns:x64-windows "gettext[tools]:x64-windows"
```

The gettext tools are build-time only. CMake uses the vcpkg-provided `msgfmt.exe` to generate `perastage.mo`, but gettext tools and DLLs are not Perastage runtime dependencies for users.

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

From the repository root, configure a Debug build:

```powershell
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

- Verify that vcpkg exists at `C:/vcpkg`.
- Verify that the required dependencies are installed with the `x64-windows` triplet.
- Remove stale build directories and reconfigure.
- If Visual Studio keeps stale state, close Visual Studio and remove the local `.vs` folder.
- Follow the detailed fix paths in [Troubleshooting](troubleshooting.md).
