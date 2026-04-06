# Windows Installation Notes

This guide covers practical setup for building and running Perastage on Windows. It is intentionally concise and focused on common successful paths.

## Recommended Toolchain

- Visual Studio 2022 with C++ desktop workload.
- CMake (bundled with Visual Studio or standalone).
- Dependency source such as vcpkg or MSYS2, depending on team preference.

## Configure and Build

From a Developer PowerShell:

```powershell
cmake -S . -B out/build/x64-Release -G "Visual Studio 17 2022" -A x64
cmake --build out/build/x64-Release --config Release
```

## Run

- Launch `Perastage.exe` from the generated build output.
- Use `perastage_stage` when preparing a packaging-ready runtime folder.

## If Configuration Fails

- Verify compiler tools from `x64 Native Tools Command Prompt` or Developer PowerShell.
- Remove stale build directories and reconfigure.
- Follow the detailed fix paths in [Troubleshooting](troubleshooting.md).
