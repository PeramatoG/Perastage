# Perastage v1.5.0 Release Notes

Changes since **v1.4.0**.

Perastage 1.5.0 is a substantial update focused on GDTF and truss editing, multilingual support, MVR-xchange interoperability, workflow improvements, and application stability.

## Highlights

- Added an early implementation of **MVR-xchange TCP Mode** for discovering compatible applications, requesting MVR files, and manually publishing the current scene.
- Added the new **Edit Truss** dialog and significantly expanded the GDTF information available in **Edit Fixture**.
- Added **Cross-table Actions**, allowing compatible 2D and 3D tools to work across fixtures, trusses, hoists, and scene objects.
- Added a **Scene Object to Truss** conversion tool.
- Added a **Spanish interface**, selectable from Preferences and applied after restarting Perastage.
- Improved MVR/GDTF compatibility, 3D picking stability, Unicode handling, crash diagnostics, and release packaging.

## New features and improvements

### Interface and workflow

- Added interface-language selection for **English** and **Spanish**. The selected language is stored in Preferences and is applied after restarting the application.
- Added **Cross-table Actions** to the viewport toolbar. When enabled, hover, selection, measurement, and compatible tools can work across all supported object types instead of being limited to the active Data Views table.
- Added a **Gap Measure Tool** to the viewport toolbar for measuring the nearest edge-to-edge space between two scene objects, complementing the existing center-to-center measurement tool.
- Updated the center-to-center **Measure Tool** toolbar icon to use a dedicated Lucide dimension-ruler artwork, keeping it visually distinct from the Gap Measure Tool icon.
- Added a persistent **Local Axes** viewport toolbar toggle and `--local`/`-l` command-bar modifiers for relative position and rotation transforms, while keeping world axes as the default.
- Corrected the **Local Axes** toolbar icon to use the official Lucide `file-axis-3d` artwork.
- Added a shared **Hang Position editor** for fixtures, trusses, and hoists. Positions can be selected, created, renamed across affected objects, or removed from one dialog.
- Updated Data Views interactions:
  - Double-clicking with the left mouse button opens cell-specific editing actions.
  - Double-clicking a cell inside an existing multi-selection now keeps that selection and applies the existing bulk-edit behavior to all selected rows without requiring Shift.
  - Right-clicking opens row-level editors and context actions.
- Edit Fixture and Edit Truss can now be maximized and use resizable layouts whose proportions are remembered between sessions.

### Fixture and truss editing

- Added a dedicated **Edit Truss** dialog with:
  - MVR instance properties.
  - GDTF identity and metadata.
  - Physical properties and dimensions.
  - A reusable 3D preview.
  - Support for creating a Perastage-generated GDTF when compatible metadata is edited on a model-only truss.
- Expanded **Edit Fixture** with a clearer and more structured GDTF inspection interface:
  - More informative top-level DMX channel labels in the mode/channel browser for faster scanning.
  - DMX modes and channels.
  - Effective channel addresses for matrix fixtures.
  - Logical and channel functions.
  - DMX and physical ranges.
  - Active functions and channel sets for selected DMX values.
  - Wheel, slot, filter, media, and graphic-wheel information.
- Added visual previews for GDTF wheel slots, including gobo thumbnails and approximate color or filter swatches.
- Improved fixture and truss preview layouts with larger metadata areas, official GDTF SVG symbols when available, generated Top/Front/Side symbols, and more consistent dark-theme presentation.
- Improved Perastage-generated and normalized truss GDTF files:
  - More consistent archive names and metadata.
  - Better preservation of edited values after saving and reopening.
  - Shared type metadata for trusses that use the same source.
  - Correct conversion of physical dimensions to the active interface units.

### Scene Object to Truss conversion

- Added **Scene Object to Truss** to the Tools menu and the 2D/3D scene-object context menus.
- The command converts all scene objects that use the selected model file into trusses and updates tables, viewports, and rigging calculations.

### MVR-xchange — early implementation

- Added an initial **MVR-xchange TCP Mode** implementation for compatible applications on the local network.
- Perastage can:
  - Advertise itself through mDNS/DNS-SD.
  - Discover stations in the selected MVR-xchange group.
  - Request an advertised MVR and either open it as a new project or merge it into the current project.
  - Manually publish the current scene as a new MVR revision.
  - Keep compatible peer connections available for commit announcements.
- Added interface selection, advertised address and port information, remote-station status, and a detailed transfer log to help diagnose network or compatibility problems.
- Published MVR revisions now use readable project-based file names instead of exposing internal UUIDs.

## Important fixes

- Hardened GDTF Share sign-in, credential storage, diagnostics, and download handling so passwords use the operating system credential store when available, login errors are reported more accurately, and failed downloads no longer replace existing files.
- Improved GDTF Share session handling for fixture downloads and MVR import, including safer cookie ownership, transactional replacement of existing GDTF files, strict legacy credential migration, username-only credential hints, and clearer warnings when secure password storage is unavailable.
- Improved Dictionary Editor reliability so fixture and truss edits are saved transactionally, unresolved file references remain visible and savable, dirty-state checks catch category/color/path/name changes, and failed saves keep the editor open.
- Improved Dictionary Editor asset ownership so saved truss additions and replacements ingest supported source files as owned GDTF assets, and fixture/truss resets create portable self-contained dictionaries with rollback.
- Fixed portable dictionary ZIP import so previewing or cancelling a bundle no longer copies assets into the active dictionary storage; bundle assets are staged and installed only after final confirmation with rollback for both JSON and assets.
- Fixed Dictionary Editor reset reliability for fixture and truss defaults, including legacy fixture `path` references, canonical `file` output, parser validation before installation, and safer managed-default asset replacement on Windows.
- Fixed dictionary lookups and truss dictionary loading so missing asset references remain visible for repair and no longer trigger silent saves, entry deletion, backups, or load-time truss migration.
- Improved Dictionary Editor safety by scoping dirty-edit prompts to the affected fixture or truss page, making **Discard** reload only that page, stopping saves after the first failure, and blocking writes to invalid or missing custom dictionaries until an explicit recovery action is chosen.
- Replaced the Dictionary Editor dictionary chooser with explicit **Open**, **New**, **Duplicate Current**, and **Use Default** workflows for fixture and truss dictionaries, including type validation before changing the active path and safe duplication of referenced assets.
- Simplified the Dictionary Editor controls so common actions stay visible, less frequent active-dictionary actions are grouped under **More...**, fixture GDTF download is shown only on the fixtures page, and **Export...** offers only JSON Snapshot or Portable ZIP Bundle.
- Fixed custom fixture and truss dictionaries so newly owned GDTF assets are stored in a sibling `_assets` folder and remain portable when the dictionary is moved with that folder.
- Fixed Data Views table edits so moving fixtures, trusses, or scene objects between visible and hidden layers rebuilds the relevant viewer resources, invalidates stale 3D visible-set caches, and immediately refreshes both the 2D and 3D viewports, including layout previews.
- Fixed Windows build and linker issues in the Local Axes viewport integration.

### MVR, GDTF, and project data

- Improved Windows file identity handling so differently capitalized paths to the same GDTF or truss resource share the correct cache and export information.
- Hardened layer editing and MVR persistence:
  - Layer names are validated as UTF-8.
  - Some legacy Windows-1252 name corruption can be recovered.
  - Layer edits use stable UUIDs.
  - Exported scene XML is validated before the MVR archive is written.
- Fixed MVR imports when a declared GDTF archive is missing. The fixture type remains available in the conflict resolver and can still be matched through Download GDTF.
- Improved compatibility with GDTF archives containing Unicode resource names without correct UTF-8 ZIP metadata. The original archive is left unchanged.
- Fixed MVR merge imports so incoming models, symbols, GDTF files, and other resources are copied into a valid project resource location before being referenced.
- Fixed MVR export when fixture numeric IDs are duplicated. Perastage now assigns the next available number and reports a non-blocking warning.
- Improved downloaded GDTF validation and error reporting, including clearer diagnostics for invalid or empty `description.xml` files.
- Fixed geometry-only MVR trusses so previews, names, and rigging weight warnings remain available without requiring a generated GDTF.
- Fixed shared truss weight updates and reduced unnecessary hoist-load recalculation prompts.

### GDTF inspection and previews

- Fixed wheel parsing for standard GDTF `Slot` elements.
- Fixed extensionless wheel media references and canonical `wheels/` resource lookup.
- Improved color-wheel preview conversion for commonly used CIE xyY luminance ranges.
- Improved transparent gobo presentation and added larger previews or diagnostics when selecting a wheel slot.
- Fixed matrix-fixture channel summaries so GeometryReference offsets produce the effective DMX addresses.
- Reduced expensive redraws when opening or hovering over large mode/channel trees.

### Selection, layout, and rendering

- Fixed table selection so sorting fixtures, trusses, hoists, or scene objects keeps the same UUID-backed objects selected.
- Fixed fixture multi-selection actions so the original selection order is preserved after sorting.
- Fixed Cross-table rectangle selection when using **Ctrl + left drag**.
- Fixed the new **Gap Measure Tool** bounds helpers so the 2D viewport build can resolve edge-to-edge measurement points correctly.
- Fixed Layers visibility double-clicks so they no longer open the rename dialog.
- Fixed Layout View so opening another project does not briefly reuse the previous project's cached preview.
- Improved 3D fixture picking and highlighting:
  - Malformed mesh triangles are skipped safely.
  - Incomplete ID-buffer framebuffers fall back to ray-based selection.
  - Unsafe pixel and depth reads are avoided on affected Intel Windows drivers.
  - OpenGL state is restored more defensively on Windows and macOS.
  - Recoverable picking fallbacks no longer show blocking warning dialogs.

### Application behavior and diagnostics

- Fixed the Updates dialog so **Yes** and **No** close it correctly and reminder suppression can be saved.
- Windows crash reports now include a matching `.dmp` minidump for use with the release debug symbols.
- Improved Windows version information in diagnostic reports.
- Added additional low-noise diagnostics for Viewer2D capture and MVR-xchange transfers.

## Experimental features and current limitations

### MVR-xchange

MVR-xchange support is **new and still experimental**. It currently implements a conservative subset of TCP Mode and has mainly been developed around the official protocol and compatibility testing with applications such as grandMA3.

Current limitations include:

- Publishing is manual; project edits are not synchronized automatically.
- WebSocket Mode is not implemented.
- Object-level live synchronization is not implemented.
- Compatibility may vary between applications, versions, firewall configurations, and network interfaces.

Please report interoperability problems with the MVR-xchange log and a Perastage diagnostic report whenever possible.

### GDTF editing

The shared GDTF editor architecture and the Edit Truss workflow are still under active development. Version 1.5.0 substantially improves inspection and editing, but unusual GDTF files or complex MVR roundtrips may still reveal cases that have not been tested.

Keep a backup of important MVR and GDTF files before modifying them and report any unexpected changes.

### Simplified Chinese

Simplified Chinese language support is currently an **incomplete preview**. The infrastructure and draft catalog are included for development and testing, but the translation must not yet be considered production-ready.

### macOS

The provided macOS packages are for **Apple Silicon (`arm64`)**. Separate builds are provided for macOS 15 and macOS 26.

## Build and packaging changes

- Windows PDB debug files are no longer included in the Windows installer.
- Minor releases now provide separate Apple Silicon packages for **macOS 15** and **macOS 26**.
- Debug symbols for all release platforms are grouped into one clearly marked developer-only archive.
- Localization catalogs are included and validated in packaged builds.
- Documentation has been reorganized into clearer user, developer, technical-note, and reference sections.

## Downloads and installation

Choose the package that matches your operating system:

| Operating system | Download |
|------------------|----------|
| **Windows 64-bit** | `Perastage_1.5.0_Setup.exe` |
| **macOS 15 — Apple Silicon** | `Perastage-1.5.0-macOS15-arm64.dmg` |
| **macOS 26 — Apple Silicon** | `Perastage-1.5.0-macOS26-arm64.dmg` |
| **Linux x86-64** | `Perastage-1.5.0-x86_64.AppImage` |
| **Arch Linux x86-64** | `Perastage-1.5.0-arch-x86_64.pkg.tar.zst` |

> **Do not download `Perastage-1.5.0-Debug-Symbols-Developers-Only.zip` unless requested by the developer or needed for crash analysis.**
>
> It contains debugging information and is not required to install or run Perastage.

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

Please open an issue on GitHub if you encounter a problem. Include the Perastage version, operating system, steps to reproduce the issue, and a diagnostic report from the **Help** menu whenever possible.

You can also contact the project at **perastage.app@gmail.com**.

## Improvements
- Improved GDTF revision messages so fixture and truss description edits identify FixtureType Description as the changed field instead of showing generic generation text.
- Added GDTF editor support for FixtureType descriptions and truss cross-section type metadata, including Tube output that omits truss cross-section names as required by GDTF.
- Fixed embedded Layout 2D fixture symbols so rotated GDTF fixtures choose the visible representative symbol plane and preserve the same orientation in preview and PDF export.

## Internal changes
- Dictionary loading is now read-only for valid custom fixture and truss dictionaries; default seeding, reset, and managed-default recovery are explicit operations.
