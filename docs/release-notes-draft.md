# Perastage v1.7.0 Release Notes

Changes since **v1.6.0**.

## Highlights

## New features and workflow improvements

## Compatibility, stability, and performance

## Important fixes

## Technical and packaging changes

- Isolated MVR GDTF and scene-resource resolution, metadata access, and caching behind a GUI-independent import service while preserving compatible path, mode, and fixture-category handling across supported builds.

- Separated logical MVR scene-node and hierarchy reading into an independent compiled module while preserving existing fixture, rigging, geometry, layer, and compatibility behavior.
- Preserved native Unicode filesystem paths across MVR scene-resource resolution on all supported platforms.
- Improved Windows compiler reliability for the extracted MVR scene-node reader.

- Separated MVR package acquisition and archive safety handling from semantic scene parsing to improve importer maintainability without changing supported MVR behavior.
- Corrected the informational Core/MVR coverage workflow diagnostics, dependency logging, and Linux locale preparation, with CI policy checks that prevent incomplete test environments.
- Established informational Linux coverage reporting for Core and MVR production code and expanded deterministic MVR import/export characterization ahead of future internal refactoring.
- Reconciled maintainer documentation with the active main-branch ruleset and dedicated release-App configuration.

- Added a cross-platform repository hygiene guard that blocks accidentally tracked build artifacts and unexpectedly large files while preserving the bounded bundled GDTF library as normal Git assets.
- Added an automated, cross-platform source-size guardrail that prevents existing C/C++ hotspots from growing beyond reviewable baselines and limits new source files to a maintainable default size.
- Validated the native Linux, Windows, WSL, and Apple Silicon macOS clean-checkout developer workflows, corrected Linux/WSL prerequisite installation order and Debug test dependencies, prevented Windows-only PowerShell tests from registering on Linux hosts, completed Linux system dependency and locale setup, fixed external mDNS header discovery, corrected Windows wxWidgets secure-store and Debug test-tool validation, made restricted-path policy tests use canonical directories across macOS and Windows environments, and kept local test artifacts out of the source tree.
- Prevented compatible macOS dependency caches from being discarded because binary contents were misread as SDK metadata, and migrated CI away from an immutable stale cache snapshot so repaired dependencies can persist across runs.
- Added a reproducible repository-structure baseline and policy validation to protect current module and build ownership during future organization work.
- Strengthened repository policy checks to protect third-party ownership, top-level module ownership, portable shared configuration, and explicit CMake source registration.
- Improved cross-platform reliability of the repository policy regression fixtures without weakening machine-specific path detection.
- Clarified Core ownership of the shared viewport interaction preference policy and removed its temporary repository-root compatibility exception.
- Established the shared CMake presets as the canonical local build configuration and documented optional, untracked developer overrides across supported platforms.
- Made the Windows classic-vcpkg workflow portable and reliable in Visual Studio through explicit or user-wide external checkout discovery, while ignoring the IDE's injected bundled dependency tree, keeping cross-platform validation reliable, and removing redundant legacy configuration files.
- Gave the scene-model module explicit ownership of its application source registration while preserving existing build behavior.
- Completed explicit CMake source ownership across all application modules and strengthened cross-platform, harness-aware repository checks against architecture drift.
- Moved application dependency discovery into a dedicated build module while preserving existing package-manager and platform behavior.
- Moved localization build configuration into a dedicated module while preserving existing catalog and platform behavior.
- Moved build-tree runtime asset staging into a dedicated module while preserving existing cross-platform resource layouts.
- Moved install-tree rules and packaging staging into a dedicated build module while preserving existing cross-platform installation layouts.
- Moved CPack compatibility configuration into a dedicated build module while preserving existing package metadata and installer behavior.
- Completed the build-system modularization by isolating platform target configuration and simplifying the root CMake file to project orchestration.
- Localized application include-directory ownership to feature modules while retaining shared and dependency-provided build requirements at the project level.
- Established a cross-platform, machine-checked contract for current internal module dependency directions, making accidental new coupling visible with consistent diagnostics on every supported operating system.
- Kept the Linux/WSL setup command stable while moving its detailed bootstrap workflow into a dedicated platform script with portable cross-platform compatibility checks.
- Kept the Windows setup command stable while moving Visual Studio, dependency validation, and build orchestration into dedicated platform scripts and aligning setup documentation.
- Moved application startup and lifecycle composition into a dedicated module while preserving launch, open-file, localization, diagnostics, splash, and shutdown behavior, leaving the root entry point minimal.
- Reconciled developer documentation with the current modular repository layout and clarified the authoritative references for architecture, paths, and structural policy.
- Enforced repository architecture boundaries for root entry points and source-module registration, and prevented developer-local build, preset, IDE, and machine-path configuration from being committed.
- Completed a test-backed regression audit of repository organization, build ownership, resource staging, packaging structure, documentation, and supported-platform boundaries.
- Completed the repository-organization documentation review and reconciled the merged CI and hosted packaging evidence while preserving an explicit distinction between verified builds, non-publishing dry runs, and real releases.

## Downloads and installation

Choose the package that matches your operating system:

| Operating system | Download |
|---|---|
| **Windows 64-bit** | `Perastage_1.6.0_Setup.exe` |
| **macOS 15 — Apple Silicon** | `Perastage-1.6.0-macOS15-arm64.dmg` |
| **macOS 26 — Apple Silicon** | `Perastage-1.6.0-macOS26-arm64.dmg` |
| **Linux x86-64** | `Perastage-1.6.0-x86_64.AppImage` |
| **Arch Linux x86-64** | `Perastage-1.6.0-arch-x86_64.pkg.tar.zst` |

> **Do not download `Perastage-1.6.0-Debug-Symbols-Developers-Only.zip` unless it is requested for crash analysis or you specifically need the developer debug information.**
>
> This archive is not required to install or run Perastage.

### Windows

Windows SmartScreen may warn that the application is from an unknown publisher because Perastage is an independent open-source project and is not currently code-signed.

Select **More info**, then **Run anyway** to continue.

### macOS

Perastage is not currently notarized by Apple. If macOS blocks the first launch:

1. Open **System Settings → Privacy & Security**.
2. Select **Open Anyway** for Perastage.
3. Confirm that you want to open the application.

### Linux

The AppImage may need executable permission:

```bash
chmod +x Perastage-1.6.0-x86_64.AppImage
```

### Arch Linux

Install the package with:

```bash
sudo pacman -U Perastage-1.6.0-arch-x86_64.pkg.tar.zst
```

## Need help?

Please open a GitHub issue if you encounter a problem. Include the Perastage version, operating system, clear steps to reproduce the issue, and a diagnostic report from the **Help** menu whenever possible.

You can contact the project at **perastage.app@gmail.com**.
