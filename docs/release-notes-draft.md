# Perastage v1.5.0 Release Notes

Changes since **v1.4.0**.

## Highlights

## New features

- Added the first phase of desktop localization, including a persistent interface-language preference, restart-based language switching, packaged gettext catalog resources, and a minimal Spanish catalog for the new language settings.
- Added an Edit Truss dialog from double-clicking the Trusses table, including MVR instance fields, GDTF metadata, a reusable 3D preview arranged above the GDTF truss type fields, and automatic Perastage GDTF creation when type metadata is edited on a model-only truss.
- Added MVR-xchange remote file requests with a larger selectable advertised-MVR list, Console-styled transfer log, corrected station-name alignment, and the standard import choice to open as a new project or merge into the current project.
- Added the first MVR-xchange TCP Mode publisher so Perastage can manually publish the current scene as an MVR revision for compatible clients.

## Improvements

- Added maximize and restore support to the Edit Fixture and Edit Truss dialogs for more flexible editing on large screens.
- Added visual GDTF wheel previews in Fixture Edit, including static gobo/graphic-wheel thumbnails and approximate color/filter swatches for wheel slots.
- Refined Fixture Edit DMX inspection with structured wrapping detail panels that update values in place while dragging the slider, per-channel slider value memory during the edit session, a cleaner wheel-slot preview summary, no duplicate mode-browser detail pane, and resolution-matched slider ranges, so exact DMX values, selected mode-browser details, percentages, and active function or set ranges stay readable and aligned with the selected channel.

- Connected the GDTF Wheel and Attribute Inspector to Fixture Edit so selecting a channel and moving the read-only DMX slider shows the active function, set, wheel, slot, media, filter, and graphic-wheel information in a new visual-column wheels page.

- Added a read-only hierarchical GDTF mode and channel browser with DMX ranges, physical ranges, units, details, parser diagnostics, cached source loading, and a larger quick channel summary below Physical properties with per-channel rows and readable normalized channel functions for Fixture Edit.
- Made Truss Edit open more compactly and gave the official Fixture SVG symbol region the same vertical priority as the generated Top, Front, and Side symbol previews.
- Improved Truss Edit spacing and composition so type and metadata stay in the second column while the 3D preview sits above physical properties in the third column.
- Improved Fixture Edit visual resources by combining 3D preview and fixture image into one Preview tab, adding an official GDTF SVG symbol preview above Perastage-generated symbols when available, and increasing GDTF column spacing for readability.
- Moved Fixture Edit's Mode channels summary into the Fixture instance pane and let GDTF metadata use the freed overview-column height for longer text.
- Refined Fixture Edit and Truss Edit GDTF editor layouts with compact instance panes, resizable splitters, flatter GDTF sections, larger metadata and preview areas, Fixture visual-resource tabs, and persisted layout preferences while preserving existing apply behavior.


- Improved exported truss GDTF files so Perastage-generated or Perastage-normalized trusses use canonical `Manufacturer@Model@Perastage.gdtf` archive names, stricter GDTF truss structure metadata, and preserve edited truss values across save/reopen roundtrips, share edited type metadata across trusses from the same source, and show the active GDTF in the truss model-file column when available, while keeping model-file replacements scoped to the selected trusses.
- MVR-xchange now labels the default Perastage station with the local computer name, preferring the full host name when available, making it easier to identify in other applications.

## Fixes

- Fixed Spanish localization startup so the proof strings load from the generated catalog on Windows, with native language names shown safely, required build-time catalog generation when localization is enabled, and clearer catalog diagnostics.
- Fixed the language selector build on wxWidgets configurations that require literal translation message IDs.
- Fixed Wheel slots selection emphasis so highlighting no longer changes thumbnail or swatch colors.
- Fixed GDTF wheel color swatches so CIE xyY luminance values authored on the common 0-100 scale are normalized before sRGB preview conversion, avoiding washed-out or incorrectly white wheel colors.
- Fixed GDTF wheel PNG previews so failed decodes are reported separately from placeholders, standard `wheels/<MediaFileName>.png` resources are preferred, cached extracted GDTF resource folders from restored MVR/project data are used as a safe read-only fallback, transparent gobo artwork is composed over a visible checkerboard background, clicking a wheel-slot row shows a larger slot preview or diagnostic directly, and repeated image-handler initialization no longer triggers wxWidgets debug assertions.

- Fixed GDTF wheel PNG preview decoding by initializing wxWidgets image handlers before thumbnail creation.

- Fixed GDTF wheel media lookup so extensionless `MediaFileName` values resolve to canonical wheel resource files such as `wheels/name.png`, restoring gobo and graphic-wheel thumbnails.

- Fixed GDTF wheel parsing for fixtures that use standard `Slot` elements, enabling gobo thumbnails and color swatches to appear for those wheel slots.

- Fixed the GDTF wheel preview panel build by including the wxWidgets static-bitmap declaration used by the active slot preview.

- Fixed a Windows build conflict between the new GDTF wheel catalog types and the existing GDTF description snapshot wheel types.

- Fixed the internal GDTF resource bitmap cache header so it uses the portable wxWidgets size declaration on Windows builds.

- Fixed the GDTF editor layout helper declarations so the split-pane layout builds correctly on Windows toolchains.
- Kept the GDTF mode/channel browser responsive by avoiding expensive nested container-column redraws during Fixture Edit opening and mouse hover.
- Removed the experimental Channel function column from the GDTF mode/channel browser because the quick Mode channels summary already provides the useful channel-function scan.
- Fixed GDTF mode/channel summaries for matrix fixtures by applying GeometryReference DMX offsets, so repeated RGBW pixel channels appear at their effective addresses instead of restarting at channel 1.
- Matched Fixture Edit thumbnail and symbol preview margins to the dialog background instead of forcing white canvases, improving dark-theme and themed-layout appearance.

- Fixed Fixture Edit GDTF source resolution so project-relative fixture references are resolved before metadata, modes, channels, previews, symbols, thumbnails, and GDTF mutation paths use them, preventing bare file-name open errors after the Checkpoint 08A session binding.
- Fixed GDTF imports with Unicode resource filenames whose ZIP entries omit UTF-8 filename metadata, allowing affected fixtures to insert with their real models, refresh tables and viewports immediately, reopen without stalling during mode resolution, and report a concise compatibility warning while leaving the original archive unchanged.
- Fixed adding downloaded GDTF Share fixtures to a project by validating the archive with non-throwing read diagnostics before opening Add Fixture, while preserving the downloaded file unchanged.
- Improved downloaded GDTF fixture insertion compatibility by preserving WiringObject-based power fallback metadata, resolving wheel media resources from standard `wheels/` archive entries, and reporting empty `description.xml` files with clearer diagnostics.
- Fixed geometry-only MVR trusses so rigging warnings reliably flag invalid weights, Edit Truss previews direct GLB/3DS geometry without generating a GDTF, and Add Truss shows readable names instead of internal type keys.
- Fixed Layout View so opening another project cannot briefly reuse the previous project's cached layout preview before the new layout rebuilds.
- Fixed table selection highlights so sorting fixtures, trusses, hoists, or scene objects keeps the same UUID-backed elements selected, and fixture multi-selection actions preserve the original selection order after sorting.
- Fixed Layers visibility checkbox double-clicks so they only toggle visibility and no longer open the layer rename dialog.
- Fixed MVR export so duplicate fixture numeric IDs are repaired with the next available number, logged as non-blocking warnings, and no longer prevent saving the MVR file.
- Fixed first-attempt truss weight edits by propagating shared truss type weights before scene synchronization and hoist-load recalculation.
- Fixed the Trusses table so hoist-load recalculation prompts only appear when an edit can change a position rigging total, such as weight or hang-position changes, instead of dimension-only edits like width or height.
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

- Hardened the internal GDTF editor session model so unsupported project, MVR, derived, and host-only fields can no longer be accepted as silent edits, with dirty tracking now tied to explicit context capabilities and read-only contexts reporting truthful read-only field capabilities.
- Added local, low-noise diagnostics that identify the Viewer2D RGBA capture backend in manual diagnostic reports, including capture counts, last size, and fallback or failure reasons without changing rendering behavior.
- Hardened MVR-xchange TCP Mode protocol handling with stricter UUID validation, safer malformed-message responses, bounded latest-revision requests, non-empty payload checks, archive sanity checks, clearer transfer diagnostics, and additional deterministic protocol tests.
- Disabled optional depth-read picking by default and skipped it on Windows Intel OpenGL drivers to avoid unsafe depth-buffer reads during normal selection.
- Hardened 3D hover picking plus hover, group, and selected highlight rendering to avoid unsafe OpenGL pixel reads and restore critical render state after overlay highlights on Intel Windows drivers and macOS.
- Hardened 3D picking coordinate validation to avoid unsafe OpenGL reads near viewport edges and during zero-sized or out-of-range viewer states.
- Improved Windows crash dumps so native access violations are captured from the original exception context before best-effort text stack reporting.

## Internal changes

- Added internal GDTF Wheel and Attribute Inspector architecture for read-only wheel, slot, filter, graphic-wheel, CIE color, resource, and DMX value inspection.
- Stabilized GDTF editor Apply transactions for Fixture and Truss editing so project changes are committed only after adapter success, with clearer undo ordering, derivative reconciliation, and UTF-8 path handling.

- Improved the internal GDTF editor architecture by moving Project Fixture apply decisions toward a non-GUI adapter, adding structured apply results, preserving per-field validation errors, and making derived Channel Count dirty tracking reversible while leaving Truss apply migration for the next checkpoint.

- Completed the GDTF editor Checkpoint 06 composition step by adding a reusable presentation-only `GdtfEditorPanel` that arranges the existing metadata, type identity, physical properties, and modes panels without changing Fixture Edit or Truss Edit behavior.
- Completed the GDTF editor Checkpoint 07 host-composition migration by moving Fixture Edit and Truss Edit onto the reusable presentation-only `GdtfEditorPanel` while keeping project apply, mutation, preview, undo, and viewer behavior host-owned, and corrected static-box parenting so the migrated dialogs open without wxWidgets containment warnings.
- Completed the GDTF editor Checkpoint 08A host-session binding by making Fixture Edit and Truss Edit use existing `GdtfEditSession` state, validation, and reversible dirty tracking for supported GDTF/context fields while keeping all write/apply side effects on the legacy host paths.
- Completed the GDTF editor Checkpoint 05 panel extraction by adding reusable physical properties, type identity, and modes panels while preserving existing Fixture Edit and Truss Edit apply behavior.

- Improved GDTF geometry loading and symbol-cache validation by sharing one cached geometry build, caching primitive meshes by dimensions, reusing fixture bounds during symbol generation, and switching generated-symbol manifests to a versioned semantic GDTF fingerprint that is stable across ZIP repackaging.

- Started the reusable GDTF editor GUI migration by sharing the read-only metadata panel between Fixture Edit and Truss Edit, without changing metadata loading, apply behavior, or GDTF mutation paths.

## Build, packaging and CI

- Included localization catalogs in development, installed, and packaged runtime resource layouts, with build-tree gettext catalog generation for maintainers and packaged builds.
- Added the mdns vcpkg port to installer CI dependency setup so MVR-xchange mDNS-enabled builds can configure reliably on all packaged platforms.
- Improved the MVR-xchange TCP publisher with safer dialog shutdown, specification-aligned JSON responses, commit broadcasting, and the vcpkg mdns discovery backend, explicit mDNS interface selection, and detailed TCP/protocol diagnostics, remote station tracking, and the active mDNS group discovery, group-qualified service instance names, canonical UUID reuse, non-blocking diagnostics, and outgoing join-flow pieces needed to distinguish incoming and outgoing MVR-xchange handshakes without repeated modal message boxes, and visible advertised IP/port status in the dialog.

## Documentation

- Added localization documentation covering the `ui_language` preference, catalog locations, translation boundaries, and catalog regeneration.
- Added internal documentation for tolerant GDTF Unicode ZIP filename decoding, extraction, diagnostics, path safety, and standards-compliant write expectations.
- Added Viewer2D state ownership documentation to clarify runtime-only, user preference/config, and project/Layout definition boundaries before future offscreen rendering refactors.
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

## Internal changes

- Stabilized GDTF editor Apply transactions for Fixture and Truss editing so project changes are committed only after adapter success, with clearer undo ordering, derivative reconciliation, and UTF-8 path handling.

- Improved the internal GDTF editor architecture by moving Project Fixture apply decisions toward a non-GUI adapter, adding structured apply results, preserving per-field validation errors, and making derived Channel Count dirty tracking reversible while leaving Truss apply migration for the next checkpoint.

- Isolated volatile build version and diagnostic metadata into a dedicated build-info translation unit so version-only changes no longer force broad C++ recompilation.
- Moved GDTF metadata summary loading into the shared core layer and documented the current GDTF editor architecture boundaries for future reusable editor work.
- Added a centralized read-only GDTF archive and description snapshot foundation in core, including ordered wheel/slot preservation and focused regression coverage for future editor work.
- Added non-GUI GDTF editor architecture models for document snapshots, editable values, edit sessions, source/write policy, field ownership, project fixture/truss contexts, standalone contexts, and apply-result side effects while keeping existing dialogs and write paths unchanged.

- Moved the high-level repository map into the documentation tree and aligned repository layout references and guard checks with the current module structure.
- Documented Viewer2D state ownership boundaries to prepare for separating runtime state, user preferences, and project/Layout persistent state.
- Unified and hardened Viewer2D RGBA capture around a real framebuffer target on all platforms while keeping the existing hidden-host offscreen preview architecture and a conservative back-buffer fallback.
- Centralized wxGLCanvas attribute selection, OpenGL context binding diagnostics, and GLEW initialization ownership across shared viewer components for the 3D, 2D, Layout, and Fixture Preview panels.
- Encapsulated Layout 2D view preview capture and rasterization behind focused services while preserving the existing Viewer2D-based rendering path and PDF export separation.
- Added visible, non-printing diagnostics for failed Layout 2D preview textures while keeping PDF export behavior unchanged.
- Fixed Windows build compatibility for Layout 2D preview diagnostics.

## Internal changes

- Stabilized GDTF editor Apply transactions for Fixture and Truss editing so project changes are committed only after adapter success, with clearer undo ordering, derivative reconciliation, and UTF-8 path handling.

- Completed Checkpoint 08 GDTF editor apply architecture by routing Fixture and Truss GDTF type edits through non-GUI apply adapters, preserving no-op and MVR-only behavior, and preventing failed generation or mutation from committing project state, and preserving Windows build compatibility for the new adapter host calls and project collection types.
