# Perastage v1.7.0 Release Notes

Changes since **v1.6.0**.

## Highlights

## New features and workflow improvements

## Compatibility, stability, and performance

## Important fixes

## Technical and packaging changes

- Added a reproducible repository-structure baseline and policy validation to protect current module and build ownership during future organization work.
- Strengthened repository policy checks to protect third-party ownership, top-level module ownership, portable shared configuration, and explicit CMake source registration.
- Improved cross-platform reliability of the repository policy regression fixtures without weakening machine-specific path detection.
- Clarified Core ownership of the shared viewport interaction preference policy and removed its temporary repository-root compatibility exception.
- Established the shared CMake presets as the canonical local build configuration and documented optional, untracked developer overrides across supported platforms.
- Made the Windows classic-vcpkg workflow portable and reliable in Visual Studio through explicit or user-wide external checkout discovery, while ignoring the IDE's injected bundled dependency tree, keeping cross-platform validation reliable, and removing redundant legacy configuration files.
- Gave the scene-model module explicit ownership of its application source registration while preserving existing build behavior.
- Completed explicit CMake source ownership across all application modules and strengthened repository checks against architecture drift.

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
