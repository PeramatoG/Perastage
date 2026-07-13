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

On Windows, the recommended vcpkg location for Perastage is:

```text
C:/vcpkg
```

First verify that the expected vcpkg instance exists:

```powershell
Test-Path "C:\vcpkg\vcpkg.exe"
Test-Path "C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
```

Then verify that the required packages are installed with the `x64-windows` triplet:

```powershell
C:\vcpkg\vcpkg.exe list
```

If needed, install the dependencies again:

```powershell
C:\vcpkg\vcpkg.exe install wxwidgets:x64-windows tinyxml2:x64-windows curl:x64-windows glew:x64-windows meshoptimizer:x64-windows nanovg:x64-windows podofo:x64-windows zlib:x64-windows backward-cpp:x64-windows mdns:x64-windows "gettext[tools]:x64-windows"
```

If the CMake error path contains Visual Studio's internal vcpkg, for example:

```text
C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake
```

then Visual Studio is using its own vcpkg instance instead of the recommended `C:/vcpkg` installation.

In that case:

1. Make sure the current `CMakePresets.json` uses `C:/vcpkg/scripts/buildsystems/vcpkg.cmake` for Windows presets.
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

If your vcpkg installation is intentionally located somewhere else, either edit the Windows `CMAKE_TOOLCHAIN_FILE` values in `CMakePresets.json` or create a local `CMakeUserPresets.json`.

## Export diagnostics after a crash or bug

Perastage writes local diagnostics only. It does not upload logs or crash reports automatically.

Use **Help -> Open Logs Folder** to view the local logs and crash reports folder. The current log file is named `perastage.log`, and the previous launch is kept as `perastage.previous.log`.

Default log locations are:

- Windows: `%LOCALAPPDATA%\Perastage\logs\perastage.log`
- macOS: `~/Library/Logs/Perastage/perastage.log`
- Linux: `${XDG_STATE_HOME}/perastage/logs/perastage.log`, or `~/.local/state/perastage/logs/perastage.log` when `XDG_STATE_HOME` is not set

Crash reports are written under the `crash_reports` folder inside the same logs folder. Use **Help -> Export Diagnostic Report** to create a plain-text report that includes build information, platform details, captured OpenGL information when available, and recent log lines. Share this file manually only if you are comfortable sending it to the developer.
