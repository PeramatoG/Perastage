# Perastage v1.5.0 Release Notes

Changes since **v1.4.0**.

## Highlights

## New features

- Added the first MVR-xchange TCP Mode publisher so Perastage can manually publish the current scene as an MVR revision for compatible clients.

## Improvements

## Fixes

- Improved 3D fixture picking reliability by safely skipping malformed mesh triangles during ID-buffer picking instead of crashing.
- Hardened 3D ID-buffer picking setup so incomplete OpenGL framebuffers fall back to ray-based selection.
- Simplified 3D hover and selection highlighting so highlighted objects are drawn in the normal scene pass instead of a separate overlay pass or cached framebuffer refresh.
- Fixed 3D mesh GPU draw paths so VAO element-buffer bindings are preserved during highlight, wireframe, and shaded rendering.
- Changed 3D hover and click picking to trust valid ID-buffer hits first, treat confirmed empty ID pixels as no hit, support two-sided ID picking for inverted faces, and keep depth/ray confirmation as an opt-in diagnostic fallback.
- Prevented recoverable 3D picking fallback conditions during mouse movement from showing blocking warning dialogs.
- Reset transient OpenGL state at the start of each 3D frame to prevent stale hover-highlight state from affecting subsequent macOS renders.
- Windows crash reports now include a matching `.dmp` minidump file for post-crash analysis with release `.pdb` symbols.
- Improved Windows diagnostic OS version reporting so modern Windows versions are identified more accurately.

- Fixed the Perastage Updates dialog so Yes and No close the prompt correctly and the per-version reminder suppression can be saved from the startup update prompt.

## Stability and diagnostics

- Disabled optional depth-read picking by default and skipped it on Windows Intel OpenGL drivers to avoid unsafe depth-buffer reads during normal selection.
- Hardened 3D hover picking plus hover, group, and selected highlight rendering to avoid unsafe OpenGL pixel reads and restore critical render state after overlay highlights on Intel Windows drivers and macOS.
- Hardened 3D picking coordinate validation to avoid unsafe OpenGL reads near viewport edges and during zero-sized or out-of-range viewer states.
- Improved Windows crash dumps so native access violations are captured from the original exception context before best-effort text stack reporting.

## Build, packaging and CI

- Kept MVR-xchange build includes isolated to module directories, completed dialog header dependencies, and clarified wxString conditionals so Windows builds do not shadow standard library headers or depend on ambiguous wxWidgets conversions.

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
