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

## Installed binaries vs editable user library

Perastage keeps executable binaries in the installation directory (for example,
`C:\Program Files\Perastage` when installed by Inno Setup), but editable
library data is stored in the per-user data directory.

### Editable library location (Windows)

- Default writable root: `%APPDATA%\Perastage\library\`
- Typical subfolders used by the app:
  - `%APPDATA%\Perastage\library\fixtures\`
  - `%APPDATA%\Perastage\library\trusses\`
  - `%APPDATA%\Perastage\library\scene_objects\`
  - `%APPDATA%\Perastage\library\misc\`
  - `%APPDATA%\Perastage\library\projects\`
  - `%APPDATA%\Perastage\library\default_layouts\`

Support note: users can open this location directly from
**Tools → Open user library folder** in the application.

### Bootstrap / migration behavior

At startup, Perastage runs a non-destructive bootstrap migration:

1. It reads bundled seed library content from the installed app directory.
2. It copies missing files into `%APPDATA%\Perastage\library\`.
3. It never overwrites existing user files during this bootstrap.

This lets installers update bundled defaults while preserving user-edited
library content.

### Permissions expectation

Editing library content does **not** require administrator privileges because
writes happen in the user profile tree, not inside `Program Files`.

## If Configuration Fails

- Verify compiler tools from `x64 Native Tools Command Prompt` or Developer PowerShell.
- Remove stale build directories and reconfigure.
- Follow the detailed fix paths in [Troubleshooting](troubleshooting.md).
