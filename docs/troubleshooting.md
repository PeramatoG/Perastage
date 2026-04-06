# Build Troubleshooting

This page collects common build and IDE/tooling failures for current Perastage development environments.

## Windows Linker Error: `LNK1163`

`LNK1163: invalid selection for COMDAT section` usually indicates stale or incompatible object files from a different MSVC toolset or incompatible linker settings.

### Fix Steps

1. Delete the affected build folder (for example `out/build/x64-Debug`).
2. Reconfigure with your intended generator and toolset.
3. Rebuild with a clean pass.

```powershell
cmake --build out/build/x64-Debug --config Debug --clean-first
```

## C++20 Symbols Missing in IntelliSense

Typical editor-only symptoms:

- `std has no member filesystem / optional / variant / string_view / bit_cast`
- structured-binding parse failures
- cascaded parser errors that do not match real compiler output

This usually means the editor is not attached to the active CMake configuration.

### Fix Steps

1. Remove the current build directory.
2. Reconfigure from a clean developer shell.
3. Build from that same directory.
4. Reopen/reindex IntelliSense.

```powershell
cmake -S . -B out/build/x64-Debug -G "Visual Studio 17 2022" -A x64
cmake --build out/build/x64-Debug --config Debug
```

When editor diagnostics and compiler output disagree, prioritize `cmake --build` output.

## VS Code + CMake Tools Misalignment

If VS Code shows persistent semantic errors while builds succeed:

1. Select the intended configure preset in CMake Tools.
2. Run **CMake: Configure**.
3. If needed, delete the selected preset build directory and configure again.
4. Verify C++ extension and CMake Tools extensions are installed and enabled.

## macOS Toolchain Incomplete (Apple Silicon Common Case)

Common configure failures:

- missing Ninja build program,
- `CMAKE_CXX_COMPILER not set, after EnableLanguage`.

### Fix Steps

```bash
xcode-select --install
brew install cmake ninja
xcode-select -p
clang++ --version
ninja --version
cmake --version
```

If you changed kit/generator, delete the previous build directory and configure again.

## Package-Manager and Triplet Mismatch

When using vcpkg or equivalent package managers:

- keep architecture/triplet consistent with your configure command,
- avoid mixing cached outputs from different triplets/toolchains.

## Reporting Issues Effectively

Include the following when filing a bug report:

- exact configure and build commands,
- full error output (not only summary lines),
- platform, compiler version, and generator information,
- whether the failure is compiler/linker output or editor-only diagnostics.
