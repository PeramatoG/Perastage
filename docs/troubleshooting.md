# Build Troubleshooting

This page collects common build and tooling issues that were previously embedded in the README. Use it when the baseline build commands do not succeed.

## Windows: `LNK1163` COMDAT Errors

This usually indicates stale or incompatible object files from a different toolset or linker configuration.

### Fix Steps

1. Delete the affected build directory (for example `out/build/x64-Debug`).
2. Reconfigure CMake from a clean shell.
3. Rebuild with `--clean-first` when needed.

```powershell
cmake --build out/build/x64-Debug --config Debug --clean-first
```

## Visual Studio and VS Code IntelliSense Mismatch

Symptoms often include inactive parser diagnostics about missing C++20 symbols (`optional`, `variant`, `filesystem`) while real builds still succeed.

### Fix Steps

1. Remove the active build directory.
2. Reconfigure with the intended compiler kit/preset.
3. Build from the same configured tree.
4. Wait for IntelliSense to reindex.

Compiler output from `cmake --build` is authoritative when editor diagnostics disagree.

## macOS: Missing Ninja or Compiler Setup

Typical messages include missing Ninja or unset `CMAKE_CXX_COMPILER`.

### Fix Steps

```bash
xcode-select --install
brew install cmake ninja
xcode-select -p
clang++ --version
ninja --version
cmake --version
```

If kits or generators changed, remove the previous build directory and reconfigure.

## Additional Diagnostics

- Confirm CMake preset and generator match your installed toolchain.
- Keep architecture and triplet settings consistent when using package managers.
- Capture full configure/build logs when reporting issues.
