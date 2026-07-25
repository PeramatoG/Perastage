# Perastage v1.6.0 Release Notes

Changes since **v1.5.0**.

## Highlights

## New features and workflow improvements

## Compatibility, stability, and performance

## Important fixes

## Current limitations

## Technical and packaging changes

- Fixed GitHub Packages dependency-cache initialization so clean GitHub Actions runners receive an isolated NuGet configuration before the cache source is added.

- Improved GitHub Packages dependency-cache setup with reliable structural validation and safe, redacted failure diagnostics across all supported build platforms.

- Added a secure, platform-compatible persistent dependency cache with main-only publishing for trusted GitHub Actions builds while retaining fast local workflow caches and read-only behavior for CI and installers.

- Improved GitHub Actions vcpkg caching so dependency builds are saved immediately after successful installation and can be reused across compatible CI and installer workflows.

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
