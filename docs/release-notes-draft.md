# Perastage v1.5.0 Release Notes

Changes since **v1.4.0**.

Perastage 1.5.0 is a major update focused on interoperability, truss and GDTF workflows, editing efficiency, and application reliability.

It also introduces the foundations of a multilingual interface and includes extensive internal work to improve MVR/GDTF handling, diagnostics, testing, and release packaging.

## Highlights

### MVR-xchange support

Perastage now includes an initial implementation of **MVR-xchange TCP Mode**.

MVR-xchange is an emerging interoperability workflow already being adopted by professional lighting applications. It allows compatible software on the same network to discover each other and exchange MVR revisions without relying on manual file transfers.

Perastage can now:

- Advertise itself through mDNS/DNS-SD.
- Discover compatible stations in the selected MVR-xchange group.
- Request an advertised MVR and open it as a new project or merge it into the current project.
- Manually publish the current scene as a new MVR revision.
- Maintain compatible peer connections for commit announcements.
- Display network interface information, remote-station status, and a detailed transfer log.

Published revisions use readable project-based file names instead of internal UUIDs.

> MVR-xchange support is new and experimental. See [Current limitations](#current-limitations) before using it in a production workflow.

### Expanded truss and GDTF workflows

Truss support has been significantly expanded, with particular attention to trusses represented as **GDTF files**.

Main additions include:

- A dedicated, resizable **Edit Truss** dialog with MVR instance data, GDTF metadata, physical properties, dimensions, and a reusable 3D preview.
- Support for creating a Perastage-generated GDTF when compatible metadata is edited on a model-only truss.
- Better preservation of shared type information for trusses using the same GDTF source.
- **Replace Trusses**, which swaps selected trusses while preserving their placement and scene identity.
- Replacement sources from the scene, dictionaries, GDTF, GTruss, GLB, and 3DS files.
- **Scene Object to Truss**, for converting scene objects that use the selected model into trusses.
- Context-menu actions for selecting trusses by model, source file, or hang position.
- More reliable truss selection after sorting, replacing, deleting, inserting, or reloading project data.

Geometry-only trusses remain usable when no GDTF is available, including previews, naming, and rigging weight warnings.

### Multilingual interface foundation

Perastage is being prepared for additional interface languages.

- **English**, **Spanish**, and **Simplified Chinese** language catalogs are included.
- The interface language can be selected in Preferences and is applied after restarting Perastage.
- English remains the reference language.
- Spanish and Simplified Chinese are still under review and may contain incomplete or inaccurate translations.

Translation corrections and feedback are welcome.

## User-facing improvements

### Editing and navigation

- Added **Cross-table Actions**, allowing compatible selection, hover, measurement, and editing tools to work across fixtures, trusses, hoists, and scene objects instead of being restricted to the active Data Views table.
- Added a **Gap Measure Tool** for measuring the nearest edge-to-edge distance between scene objects.
- Added a persistent **Local Axes** toggle, plus `--local` and `-l` command-bar modifiers, for relative position and rotation transforms. World axes remain the default.
- Added a shared **Hang Position editor** for fixtures, trusses, and hoists. Positions can be selected, created, renamed across affected objects, or removed from one dialog.
- Double-clicking a Data Views cell inside an existing multi-selection now preserves that selection and applies the bulk-edit behavior to all selected rows without requiring Shift.
- Right-click actions remain associated with the row that was clicked.
- **Edit Fixture** and **Edit Truss** can now be maximized, resized, and remember their layout proportions.
- The 2D View Editor now supports arrow-key panning, Alt+arrow zooming, and the `Z` fit-view shortcut while focus is in compatible side-panel controls.

### GDTF inspection and editing

**Edit Fixture** now presents GDTF information in a clearer and more detailed structure, including:

- DMX modes, channels, logical channels, and channel functions.
- More informative channel labels and effective addresses for matrix fixtures.
- DMX and physical ranges, active functions, and channel sets.
- Wheels, slots, filters, media, and graphic-wheel information.
- Gobo thumbnails and approximate color or filter previews.

Fixture and truss previews now provide larger metadata areas, official GDTF SVG symbols when available, generated Top/Front/Side symbols, and more consistent dark-theme presentation.

Additional editing improvements include FixtureType descriptions, truss cross-section metadata, clearer revision messages, cleaner physical values, and safer archive replacement when saving modified GDTF files.

### Layouts

- Replaced standalone JSON layout-template export with portable `.pslayout` packages.
- Layout packages are self-contained ZIP-based files and can include referenced images.
- Legacy JSON layout templates remain importable.
- Layout View now fits pages more reliably after opening Layout Mode, switching layouts, loading projects, changing page setup, or restoring a visible layout perspective.
- Routine content edits continue to preserve the user's zoom and pan.
- Default and active layout selections are restored more reliably after startup and project reload.
- Fixed rotated GDTF fixture symbols so their representative plane and orientation remain consistent in Layout previews and PDF exports.
- Opening another project no longer briefly reuses the previous project's cached layout preview.

### Dictionary Editor and GDTF Share

The Dictionary Editor now provides clearer **Open**, **New**, **Duplicate Current**, and **Use Default** workflows, with safer validation and transactional saving.

Portable dictionary bundles now stage their files before installation, preserve missing references for repair, and keep custom assets in a sibling `_assets` folder so dictionaries remain portable when moved together with their assets. Valid dictionaries are no longer modified silently while being loaded.

GDTF Share authentication and downloads have also been hardened:

- Passwords use the operating system credential store when available.
- Login and catalog errors provide clearer diagnostics.
- Authenticated sessions are reused more reliably.
- Existing files are not replaced by failed or invalid downloads.
- Downloaded GDTF files are validated before installation.
- Online and cached catalog states are reported more clearly.

Official release builds require native secure credential-store support on Windows, Linux, Arch Linux, and macOS.

## Compatibility, stability, and performance

### MVR, GDTF, and project data

- Improved compatibility with GDTF archives containing Unicode resource names without correct UTF-8 ZIP metadata while leaving the original archives unchanged.
- Improved Unicode filesystem handling and Windows path identity, including differently capitalized references to the same resource.
- Hardened layer editing with UTF-8 validation, stable UUIDs, and limited recovery of legacy Windows-1252 corruption.
- Exported scene XML is now validated before writing the MVR archive.
- Missing GDTF files no longer remove fixture types from the MVR conflict resolver.
- MVR merge imports now copy models, symbols, GDTF files, and other resources into valid project-owned locations before referencing them.
- Duplicate fixture numeric IDs are corrected during export with a non-blocking warning.
- Generated identifiers for scene objects, fixtures, trusses, and supports now use RFC 4122-compatible UUIDs for improved MVR interoperability.
- Downloaded GDTF validation reports invalid or empty `description.xml` files more clearly.
- Shared truss weight updates are handled more consistently, with fewer unnecessary hoist-load recalculation prompts.

### Viewers and selection

- Table and viewport selections remain attached to the same UUID-backed objects after sorting and other table changes.
- Truss selection and hover highlighting are more reliable after replacement, deletion, insertion, and reload.
- Moving objects between visible and hidden layers immediately refreshes the 2D view, 3D view, and layout previews.
- Directly selected group members and indirectly highlighted group members now use distinct highlight levels.
- Corrected Cross-table rectangle selection with **Ctrl + left drag** and Layers visibility double-click behavior.
- 3D picking now handles malformed meshes and incomplete ID-buffer framebuffers more safely, with ray-based fallback where required.
- Unsafe pixel/depth reads are avoided on affected Intel Windows drivers, and OpenGL state restoration is more defensive on Windows and macOS.
- Recoverable picking fallbacks no longer display blocking warning dialogs.

### Performance and diagnostics

- Reduced unnecessary redraws in large GDTF mode and channel trees.
- Improved fixture-symbol generation by reducing scene reloads, reusing rendering resources, sharing archive inspection, caching semantic fingerprints, and using bounded parallel conversion.
- Refactored temporary files, imports, exports, generated resources, and session caches around explicit Perastage-owned lifecycles.
- Improved shutdown reliability, especially on macOS.
- Windows crash reports now include a matching `.dmp` minidump for use with release debug symbols.
- Release debug-symbol packaging now preserves macOS dSYM bundle layout for maintainer crash analysis.
- Diagnostic reports include improved Windows version information and focused logs for Viewer2D capture and MVR-xchange transfers.
- Fixed the Updates dialog so **Yes** and **No** close it correctly and reminder suppression can be saved.

## Current limitations

### MVR-xchange

MVR-xchange currently implements a conservative subset of TCP Mode and has mainly been developed against the official protocol and compatibility testing with applications such as grandMA3.

Current limitations:

- Publishing is manual; project edits are not synchronized automatically.
- WebSocket Mode is not implemented.
- Object-level live synchronization is not implemented.
- Compatibility may vary between applications, versions, firewall configurations, and network interfaces.

When reporting an interoperability problem, include the MVR-xchange log and a Perastage diagnostic report whenever possible.

### GDTF editing

The shared GDTF editor architecture and the Edit Truss workflow are still under active development.

Version 1.5.0 significantly expands inspection and editing, but unusual GDTF files or complex MVR roundtrips may still expose untested cases. Keep a backup of important MVR and GDTF files before modifying them.

### Translations

Spanish and Simplified Chinese are available for testing but are still being reviewed. English remains the reference language when a translation is unclear or incomplete.

### macOS

The provided macOS packages support **Apple Silicon (`arm64`)**. Separate packages are provided for macOS 15 and macOS 26.

## Technical and packaging changes

- Reorganized GitHub Actions into clearer Debug CI, patch artifact, compatibility package, and transactional minor-release workflows.
- Improved dependency caching, transient vcpkg retry handling, platform toolchain validation, failure diagnostics, and Node 24-compatible actions.
- Strengthened GDTF/MVR regression tests, standards-focused fixtures, Unicode path handling, and runtime resource cleanup coverage.
- Made test and release scripts less dependent on hard-coded paths, working directories, symbolic links, ripgrep, or unresolved Python aliases.
- Corrected full-rebuild linkage and environment issues across Windows, Linux, Arch Linux, and macOS test targets.
- Generated fallback GDTF assets are now produced deterministically in the build tree and kept out of the source checkout.
- Windows PDB files are no longer included in the installer.
- Debug symbols for all release platforms are grouped into one clearly marked developer-only archive.
- Localization catalogs are included and validated in packaged builds.
- Documentation has been reorganized into clearer user, developer, technical-note, and reference sections.

## Downloads and installation

Choose the package that matches your operating system:

| Operating system | Download |
|---|---|
| **Windows 64-bit** | `Perastage_1.5.0_Setup.exe` |
| **macOS 15 — Apple Silicon** | `Perastage-1.5.0-macOS15-arm64.dmg` |
| **macOS 26 — Apple Silicon** | `Perastage-1.5.0-macOS26-arm64.dmg` |
| **Linux x86-64** | `Perastage-1.5.0-x86_64.AppImage` |
| **Arch Linux x86-64** | `Perastage-1.5.0-arch-x86_64.pkg.tar.zst` |

> **Do not download `Perastage-1.5.0-Debug-Symbols-Developers-Only.zip` unless it is requested for crash analysis or you specifically need the developer debug information.**
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
chmod +x Perastage-1.5.0-x86_64.AppImage
```

### Arch Linux

Install the package with:

```bash
sudo pacman -U Perastage-1.5.0-arch-x86_64.pkg.tar.zst
```

## Need help?

Please open a GitHub issue if you encounter a problem. Include the Perastage version, operating system, clear steps to reproduce the issue, and a diagnostic report from the **Help** menu whenever possible.

For MVR-xchange interoperability problems, also include the transfer log.

You can contact the project at **perastage.app@gmail.com**.
