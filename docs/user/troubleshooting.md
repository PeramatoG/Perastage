# Troubleshooting

If Perastage does not behave as expected, use these quick checks.

## MVR file does not open

- Confirm the file extension is `.mvr`.
- Try a different MVR file to isolate whether the issue is file-specific.
- Restart Perastage and retry import/open.

## Fixtures look missing or incorrect

- Download required profiles in **Tools -> Download GDTF**.
- Check local mappings in **Tools -> Edit dictionaries**.
- Reopen the scene after downloading missing profiles.

## Cannot find local library content

Open the location from:

- **Tools -> Open user library folder**

On Windows, the default path is:

- `%APPDATA%\Perastage\library\`

## Layout or print output is not as expected

- Recheck the scene in 2D before exporting.
- Verify table values for fixtures/trusses/hoists/objects.
- If PDF export fails, choose a writable destination, avoid empty filenames, and retry after confirming the Viewer 2D print preferences.
- Invalid numeric preferences are ignored and replaced with safe defaults, so reset preferences if exports still look inconsistent.
- Export PDF again after confirming layout adjustments.

## Scene seems inconsistent after many edits

- Save the project.
- Close and reopen it.
- Revalidate critical items in both 2D and 3D views.

## CMake cannot find a dependency through vcpkg

If CMake reports that a required dependency cannot be found, for example:

```text
Could not find a package configuration file provided by "wxWidgets"
```

or:

```text
Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)
```

or a similar error for `tinyxml2`, `CURL`, `GLEW`, `meshoptimizer`, `nanovg`, `podofo`, `Backward`, or `mdns`, CMake is probably using a different vcpkg installation than the one where Perastage dependencies were installed.

On Windows, run `setup_windows.ps1` before using Visual Studio presets. The shared presets keep `C:/vcpkg` as a fallback, while the setup script writes `CMakeUserPresets.json` for the selected checkout and repository-local `vcpkg_installed` tree. Verify the default setup-script paths from the repository root:

```powershell
Test-Path ".\.tools\vcpkg\vcpkg.exe"
Test-Path ".\.tools\vcpkg\scripts\buildsystems\vcpkg.cmake"
Test-Path ".\vcpkg_installed\x64-windows"
```

If needed, install the dependencies again through the manifest and the same installed root used by CMake:

```powershell
cd C:\path\to\Perastage
.\setup_windows.ps1 -Configuration Debug -CleanBuild
```

If the CMake error path contains Visual Studio's internal vcpkg, for example:

```text
C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake
```

then Visual Studio is using its own vcpkg instance instead of the setup-managed checkout or explicit `C:/vcpkg` fallback.

In that case:

1. Run `setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild` so `CMakeUserPresets.json` points to the selected vcpkg checkout, `vcpkg_installed`, and `VCPKG_MANIFEST_MODE=OFF`.
2. Close Visual Studio.
3. Remove stale CMake state.
4. Reopen the repository folder in Visual Studio.
5. Select the Windows configure and build presets again.

Useful cleanup commands from the repository root:

```powershell
Remove-Item -Recurse -Force .\.vs -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\win-x64-debug -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\win-x64-release -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\win-x64-debug-ninja -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\win-x64-release-ninja -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\out\build\x64-Debug -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\out\build\x64-Release -ErrorAction SilentlyContinue
```

If your vcpkg installation is intentionally located somewhere else, pass `-VcpkgRoot` to `setup_windows.ps1` or create a local `CMakeUserPresets.json` that overrides both `CMAKE_TOOLCHAIN_FILE` and `VCPKG_INSTALLED_DIR`. The shared presets keep `C:/vcpkg` only as a stable fallback; the setup-script workflow should use generated local presets for custom or repository-local paths.

## Export diagnostics after a crash or bug

Perastage writes local diagnostics only. It does not upload logs or crash reports automatically.

Use **Help -> Open Logs Folder** to view the local logs and crash reports folder. The current log file is named `perastage.log`, and the previous launch is kept as `perastage.previous.log`.

Default log locations are:

- Windows: `%LOCALAPPDATA%\Perastage\logs\perastage.log`
- macOS: `~/Library/Logs/Perastage/perastage.log`
- Linux: `${XDG_STATE_HOME}/perastage/logs/perastage.log`, or `~/.local/state/perastage/logs/perastage.log` when `XDG_STATE_HOME` is not set

Crash reports are written under the `crash_reports` folder inside the same logs folder. Use **Help -> Export Diagnostic Report** to create a plain-text report that includes build information, platform details, captured OpenGL information when available, and recent log lines. Share this file manually only if you are comfortable sending it to the developer.

## Localization catalog generation

If localization catalog generation fails during configure, build, or packaging, verify that gettext tools are installed as build-time tools and visible to CMake. On Windows, run `setup_windows.ps1` or an equivalent manifest install with the same explicit installed root passed to CMake as `VCPKG_INSTALLED_DIR`. On macOS, run `brew --prefix gettext` and add its `bin` directory to `PATH` before configuring because Homebrew gettext is keg-only. The generated `perastage.mo` catalog should be staged under `resources/locale/es/LC_MESSAGES/perastage.mo` on Windows/Linux and `Perastage.app/Contents/Resources/locale/es/LC_MESSAGES/perastage.mo` on macOS.

## GDTF Share password is not saved

Official Perastage builds require wxWidgets to be compiled with `wxUSE_SECRETSTORE`. If CMake reports that secure credential storage is missing, rebuild dependencies with the repository manifest or repair an existing Windows vcpkg tree with:

```powershell
C:\vcpkg\vcpkg.exe install "wxwidgets[secretstore]:x64-windows" --recurse
```

Then delete the affected Perastage build directory and configure again so the secure-store probe sees the rebuilt wxWidgets package. On Linux, install `libsecret-1-dev` before building wxWidgets with vcpkg. At runtime, Linux password persistence also needs a Secret Service provider such as GNOME Keyring or KWallet; a headless or minimal desktop can report the runtime store as unavailable even when the feature was compiled correctly.

### CMake reports CMAKE_CXX_COMPILER not set on Windows

Run Ninja presets from a Visual Studio Developer PowerShell or run `setup_windows.ps1`, which imports the x64 MSVC environment before configuring CMake. If this appears after a failed dependency configure, rerun `setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild` so the pinned vcpkg checkout, `vcpkg_installed` root, and CMake cache are aligned.

### LNK4272 reports x64 libraries conflicting with an x86 target

`LNK4272: library machine type 'x64' conflicts with target machine type 'x86'`, unresolved `__RTC_InitBase`, unresolved `__RTC_Shutdown`, or unresolved `_mainCRTStartup` during CMake's compiler test means the build directory captured an x86 MSVC compiler or linker while the active environment points at x64 SDK/runtime libraries. This is a mixed compiler/cache/environment problem, not a Perastage source, credential-store, or vcpkg target problem.

Use the setup script so it validates the x64 MSVC tools and refreshes only the selected build directory when an incompatible cache is detected:

```powershell
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild
```

To inspect the active compiler manually from the same shell, run:

```powershell
where cl
where link
cl
$env:VSCMD_ARG_HOST_ARCH
$env:VSCMD_ARG_TGT_ARCH
```

The compiler banner must say `for x64`, and both `VSCMD_ARG_HOST_ARCH` and `VSCMD_ARG_TGT_ARCH` must be `x64`. Deleting the selected Perastage build directory is safe; deleting `.tools\vcpkg`, `vcpkg_installed`, global vcpkg downloads, or a shared developer vcpkg checkout is not required for this compiler-cache problem.

If multiple Visual Studio installations are present, select one explicitly:

```powershell
.\setup_windows.ps1 -Configuration Debug -VisualStudioPath "C:\Program Files\Microsoft Visual Studio\2022\Community" -CleanBuild -SkipBuild
```

You can also use `-VisualStudioVersion` with a vswhere-compatible version range when you prefer version selection over a full path.
