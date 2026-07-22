# Perastage v1.5.0 Release Notes

Changes since **v1.4.0**.

Perastage 1.5.0 is a substantial update focused on GDTF and truss editing, multilingual support, MVR-xchange interoperability, workflow improvements, and application stability.

## Highlights

- Added an early implementation of **MVR-xchange TCP Mode** for discovering compatible applications, requesting MVR files, and manually publishing the current scene.
- Added the new **Edit Truss** dialog and significantly expanded the GDTF information available in **Edit Fixture**.
- Added **Cross-table Actions**, allowing compatible 2D and 3D tools to work across fixtures, trusses, hoists, and scene objects.
- Added a **Scene Object to Truss** conversion tool.
- Added 2D and 3D viewer context-menu shortcuts for selecting trusses by model/source file or hang position from the **Trusses** table.
- Added a **Spanish interface**, selectable from Preferences and applied after restarting Perastage.
- Improved MVR/GDTF compatibility, 3D picking stability, Unicode handling, crash diagnostics, and release packaging.
- Improved Debug CI stability by rebuilding stale macOS SDK caches instead of failing on equivalent SDK aliases.

## New features and improvements

### Interface and workflow

- Layout Viewer now fits pages reliably after opening Layout Mode, switching layouts, loading projects, changing page setup, or restoring a visible layout perspective, while preserving user zoom and pan during routine content edits.
- Replaced standalone JSON layout-template export with portable `.pslayout` packages. Layout packages are ZIP-based, self-contained, include referenced layout images, and remain importable alongside legacy JSON templates for compatibility.
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
- The 2D View Editor now uses the same arrow-key pan and Alt+arrow zoom navigation as the standalone 2D viewport, even when focus is in the editor side panels.
- The 2D View Editor now also honors the `Z` fit-view shortcut when focus is in non-editable editor controls.

### Fixture and truss editing

- Added **Replace trusses** to the Edit menu for swapping selected trusses with a scene truss, dictionary truss, GDTF truss, GTruss archive, GLB, or 3DS model while preserving instance placement and identity.
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

- Fixed the GDTF canonicalizer test so it builds with TinyXML2 versions whose document type is non-copyable.
- Fixed the GDTF canonicalizer test target so mutation-audit build metadata resolves during full rebuilds.
- Fixed GDTF loader regression tests so they include the real viewer loader API instead of the lightweight test stub during full rebuilds.
- Fixed Windows builds that could accidentally include a local project `version` file while third-party JSON feature detection probes standard library headers.
- Fixed the symbol cache manifest test so Windows rebuilds see the standard file-stream declaration.
- Fixed the truss loader validation test target so it links the loader implementation, build-info stub, and supporting core sources during full rebuilds.
- Fixed the MVR merge analyzer/applier test target so it links runtime storage support required by merge resource workspace handling.
- Fixed the fixture label override reconciliation test target so layout template import/export dependencies link during full rebuilds.
- Fixed GDTF dictionary color tests so layout template import/export dependencies link during full rebuilds.
- Fixed active dictionary workflow and dictionary seed backup test targets so layout template dependencies link during full rebuilds.
- Fixed the layout template package service test target so layout image registry dependencies link during full rebuilds.
- Fixed project roundtrip and truss path regression test targets so MVR, layer, UTF-8, grouping, and truss loader dependencies link during full rebuilds.
- Fixed MVR address, support userdata, and layer appearance test targets so build metadata, layer validation, UTF-8, and grouping dependencies link during full rebuilds.
- Fixed additional MVR roundtrip and exporter test targets so full rebuilds link build metadata, layer validation, UTF-8 helpers, grouping synchronization, primitive bounds, and GDTF resource key helpers.
- Fixed rider import and save roundtrip test targets so dictionary lookup stubs and project MVR import/export helpers link during full rebuilds.
- Fixed GDTF loader test targets so runtime storage logging, build metadata, and archive extraction dependencies link during full rebuilds.
- Fixed symbol fixture applier and patched GDTF export test targets so MVR, layer, UTF-8, build metadata, and GDTF archive dependencies link during full rebuilds.
- Fixed the layout image resource registry test target so layout package import/export and runtime storage dependencies link during full rebuilds.
- Fixed dictionary reset and layer service UTF-8 test targets so app paths, layout package, runtime storage, and focused ConfigManager layer-selection stubs link during full rebuilds.
- Fixed dictionary reset and GDTF Share security test targets so app-path stubs and lightweight ConfigManager dependencies no longer conflict with full rebuild linkage.
- Improved portable layout package export with images by using canonical ZIP entry paths, clearer archive validation diagnostics, and side-effect-free export self-validation. The Layout panel now restores and reapplies an active layout selection when layouts exist so default layouts render after startup and reload and export, rename, and delete actions do not silently do nothing after selection loss.
- Hardened GDTF Share sign-in, credential storage, diagnostics, and download handling so passwords use the operating system credential store when available, login errors are reported more accurately, and failed downloads no longer replace existing files.
- Improved GDTF Share session handling for fixture downloads and MVR import, including clearer online-versus-cached catalog status, reuse of authenticated catalog sessions for downloads, safer cookie ownership, transactional replacement of existing GDTF files, strict legacy credential migration, username-only credential hints, and clearer warnings when secure password storage is unavailable.
- Updated official Windows, Linux, Arch, and macOS build paths to require wxWidgets secure credential-store support so GDTF Share passwords can persist through the native platform store in release builds.
- Finalized secure credential-storage release gates by separating CI manifest dependency installs from the local Windows classic `C:/vcpkg` workflow, preventing Visual Studio/CMake from rebuilding vcpkg packages during local configure, validating Windows Ninja x64 MSVC environments, adding focused credential tests to CI, and documenting Windows Credential Manager validation.
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
- Hardened truss selection identity so sorting, replacement, deletion, insertion, reloads, and hover highlights stay attached to persistent scene UUIDs across the Trusses table and both 2D and 3D viewers.
- Improved generated scene-object, fixture, truss, and support identifiers to use RFC 4122-compatible UUIDs for better MVR interoperability.
- Fixed Data Views highlight rendering on wxWidgets builds where row-to-item conversion is exposed through the list-store item API.
- Improved 2D and 3D group-selection highlighting so directly selected items remain bright cyan while other members of the same selected group use a darker blue highlight.
- Fixed Windows build and linker issues in the Local Axes viewport integration.
- Fixed a debug-only shutdown warning caused by attempting to make a hidden 2D OpenGL canvas current during cleanup.

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

- Strengthened the internal test foundation with portable Python resolution, clearer CTest labels, and documented fixture categories for standards-focused GDTF and MVR tests.

- Added explicit wxWidgets filesystem path conversion utilities for safer Unicode path handling at file API boundaries, including native wide-path conversion on Windows.
- Made layout z-order updates and legacy layer-name repair deterministic for cross-platform Debug tests.
- Hardened Windows Debug test execution by preparing ripgrep separately, forcing Git Bash for shell checks, routing shell tests through one CMake helper, and disabling modal assertion reporting in test executables.
- Corrected Debug test expectations and lifetimes for Simplified Chinese localization fallback checks and credential metadata cleanup on Windows.


- Improved Debug CI reliability by validating CMake toolchain metadata from generated files, declaring Linux test tools and locales explicitly, running Linux CTest under a verified Xvfb display, bounding non-interactive Windows CTest runs, normalizing repository-policy test working directories, and keeping CMake language policy checks focused on first-party source.
- Strengthened CI release-gate tests so policy scripts are portable without ripgrep and credential metadata checks validate JSON fields without flagging safe backend identifiers.
- Reorganized GitHub Actions into separate Debug CI, main patch artifact, compatibility package, and transactional minor release workflows, with Debug dependency setup aligned to the pinned vcpkg toolchain and Windows CI preserving the Python and MSVC environments across steps.

- Hardened installer CI configuration so test-enabled builds configure C and C++ from the project root, Windows secure-store Ninja jobs use the MSVC x64 toolchain, and final failure diagnostics include modern CMake configure logs.
- Hardened installer and release CI dependency installation with transient vcpkg retry handling, separated caches, improved failure diagnostics, and Node 24-compatible GitHub Actions.
- Improved fixture symbol generation performance by avoiding redundant 2D scene reloads during orthographic capture, reusing offscreen framebuffer resources, memoizing unchanged GDTF semantic fingerprints in-process, and converting independent rendered symbol views in bounded parallel workers while preserving exact generated geometry.
- Refactored runtime temporary storage so imports, exports, generated resources, and session caches use explicit Perastage-owned lifecycles.
- Dictionary loading is now read-only for valid custom fixture and truss dictionaries; default seeding, reset, and managed-default recovery are explicit operations.
- Corrected the generated Dummy 1ch fallback fixture pipeline so deterministic GDTF output is produced in the build tree, staged and installed as a runtime asset, and kept out of the source checkout.
- Strengthened GDTF test fixtures and Python test interpreter checks, including one-address DMX validation, isolated dictionary test libraries, and interpreter paths containing spaces.
