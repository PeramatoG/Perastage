# Perastage v1.5.0 Release Notes

Changes since **v1.4.0**.

## Highlights

## New features

- Added MVR-xchange remote file requests with a larger selectable advertised-MVR list, Console-styled transfer log, corrected station-name alignment, and the standard import choice to open as a new project or merge into the current project.
- Added the first MVR-xchange TCP Mode publisher so Perastage can manually publish the current scene as an MVR revision for compatible clients.

## Improvements

- Improved exported truss GDTF files so Perastage-generated or Perastage-normalized trusses use canonical `Manufacturer@Model@Perastage.gdtf` archive names and stricter GDTF truss structure metadata.
- MVR-xchange now labels the default Perastage station with the local computer name, preferring the full host name when available, making it easier to identify in other applications.

## Fixes

- Fixed MVR merge imports so incoming fixture, truss, hoist, scene-object, and symbol resources are copied into a valid merge resource root before references are applied, including when merging into an empty or unsaved scene.
- Fixed the MVR-xchange log text styling so transfer messages use the same readable Console colors and monospaced font.
- Fixed MVR-xchange remote imports so merging keeps the current project name, while opening a requested MVR as a new project uses the advertised MVR file name.
- Fixed MVR-xchange mDNS advertisement so the service responder binds to the selected advertised interface, improving visibility in peer service lists when multiple local interfaces are available.
- Improved MVR-xchange compatibility with clients such as grandMA3 by keeping joined TCP Mode peer connections available for immediate publish broadcasts, using a grandMA3 endpoint fallback when discovery only reports an incoming join, refreshing discovery before manual publishes, and sending each join refresh and commit announcement over the same TCP connection before waiting for acknowledgements.
- Fixed MVR-xchange station naming so generated default names use the full Windows DNS computer name without slow reverse lookups, repair older truncated defaults, and no longer open with the station-name field selected or horizontally scrolled.
- Simplified MVR-xchange file names so manual publishes use a readable project-name-and-timestamp pattern instead of exposing the internal file UUID.
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

- Hardened MVR-xchange TCP Mode protocol handling with stricter UUID validation, safer malformed-message responses, bounded latest-revision requests, non-empty payload checks, archive sanity checks, clearer transfer diagnostics, and additional deterministic protocol tests.
- Disabled optional depth-read picking by default and skipped it on Windows Intel OpenGL drivers to avoid unsafe depth-buffer reads during normal selection.
- Hardened 3D hover picking plus hover, group, and selected highlight rendering to avoid unsafe OpenGL pixel reads and restore critical render state after overlay highlights on Intel Windows drivers and macOS.
- Hardened 3D picking coordinate validation to avoid unsafe OpenGL reads near viewport edges and during zero-sized or out-of-range viewer states.
- Improved Windows crash dumps so native access violations are captured from the original exception context before best-effort text stack reporting.

## Build, packaging and CI

- Added the mdns vcpkg port to installer CI dependency setup so MVR-xchange mDNS-enabled builds can configure reliably on all packaged platforms.
- Improved the MVR-xchange TCP publisher with safer dialog shutdown, specification-aligned JSON responses, commit broadcasting, and the vcpkg mdns discovery backend, explicit mDNS interface selection, and detailed TCP/protocol diagnostics, remote station tracking, and the active mDNS group discovery, group-qualified service instance names, canonical UUID reuse, non-blocking diagnostics, and outgoing join-flow pieces needed to distinguish incoming and outgoing MVR-xchange handshakes without repeated modal message boxes, and visible advertised IP/port status in the dialog.

## Documentation

- Updated the MVR-xchange documentation with a compliance summary, supported official flows, conservative latest-request behavior, and explicit out-of-scope notes for WebSocket Mode and private live synchronization.
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
