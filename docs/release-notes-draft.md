# Perastage v1.5.0 Release Notes

Changes since **v1.4.0**.

## Highlights

## New features

## Improvements

## Fixes

- Improved 3D fixture picking reliability by safely skipping malformed mesh triangles during ID-buffer picking instead of crashing.
- Hardened 3D ID-buffer picking setup so incomplete OpenGL framebuffers fall back to ray-based selection.
- Fixed macOS 3D hover highlighting so overlay rendering leaves fixed-function texture unit 0 in a safe unbound state.
- Windows crash reports now include a matching `.dmp` minidump file for post-crash analysis with release `.pdb` symbols.
- Improved Windows diagnostic OS version reporting so modern Windows versions are identified more accurately.

- Fixed the Perastage Updates dialog so Yes and No close the prompt correctly and the per-version reminder suppression can be saved from the startup update prompt.

## Stability and diagnostics

- Hardened 3D hover picking plus hover, group, and selected highlight rendering to avoid unsafe OpenGL pixel reads and restore critical render state after overlay highlights on Intel Windows drivers and macOS.
- Hardened 3D picking coordinate validation to avoid unsafe OpenGL reads near viewport edges and during zero-sized or out-of-range viewer states.
- Improved Windows crash dumps so native access violations are captured from the original exception context before best-effort text stack reporting.

## Build, packaging and CI

## Documentation

## Compatibility notes

## Downloads and installation

Perastage is available for Windows, macOS, Linux (AppImage) and Arch Linux.

Choose the package that matches your operating system:

| Operating system | Download |
|------------------|----------|
| **Windows** | `Perastage-1.4.0_Setup.exe` |
| **macOS (Apple Silicon)** | `Perastage-1.4.0-macos-arm64.dmg` |
| **Linux** | `Perastage-1.4.0-x86_64.AppImage` |
| **Arch Linux** | `Perastage-1.4.0-arch-x86_64.pkg.tar.zst` |

> **Do not download the `*-symbols.zip` files unless you have been asked to do so by the developer.**
> These files contain debugging symbols used to investigate crash reports and are **not required** to install or run Perastage.

### Installation notes

#### Windows

Windows SmartScreen may warn that the application is from an unknown publisher because Perastage is an independent open-source project and is not code-signed.

If this happens:

- Click **More info**.
- Click **Run anyway**.

Some antivirus products may also perform an additional scan the first time the installer is executed. This is normal for newly released software.

#### macOS

Because Perastage is not currently notarized by Apple, macOS may prevent it from opening the first time.

If this happens:

- Open **System Settings > Privacy & Security**.
- Click **Open Anyway** for Perastage.
- Confirm that you want to run the application.

This only needs to be done once.

#### Linux

The AppImage may need to be marked as executable before running:

```bash
chmod +x Perastage-1.4.0-x86_64.AppImage
```

Then launch it normally.

#### Arch Linux

Install the package using:

```bash
sudo pacman -U Perastage-1.4.0-arch-x86_64.pkg.tar.zst
```

## Need help?

If you encounter any problems installing or running Perastage, please open an issue on GitHub or contact the developer. Including a diagnostic report (available from the Help menu) makes it much easier to investigate problems.
