# Perastage v1.6.0 Release Notes

Changes since **v1.5.0**.

Perastage 1.6.0 is a major workflow and reliability update focused on faster scene creation, more practical fixture placement, improved rider import, stronger MVR interoperability, and a smoother day-to-day editing experience.

## Highlights

### Scene Cut, Copy, and Paste

- Added full scene **Cut, Copy, and Paste** support for fixtures, trusses, hoists, and scene objects.
- Standard `Ctrl+X`, `Ctrl+C`, and `Ctrl+V` shortcuts are available from the Edit menu and toolbar.
- Single-item Paste supports repeated pointer-driven placement in both 2D and 3D views.
- Multi-item selections can be pasted as a rigid group while preserving their relative positions.
- Clipboard placement integrates with Undo/Redo and safely preserves hierarchy, fixture labels, and project identities.

### New fixture distribution tools

- Added a unified **Distribute fixtures** workflow on `Alt+D`.
- Fixtures can now be distributed:
  - uniformly across a complete straight truss,
  - between two selected points,
  - with an exact center-to-center or edge-to-edge spacing,
  - outside-in between two limits,
  - or from a selected starting point in a chosen direction.
- Distribution uses the actual resolved truss attachment line and loaded fixture dimensions where required.
- All distribution operations support Undo/Redo and provide non-blocking feedback when the requested arrangement does not fit.

### Fixtures now attach to real truss mounting lines

- Fixtures can now snap continuously along the main structural chords of straight square, triangular, and ladder trusses instead of relying on isolated connector points.
- Perastage prefers the structural model referenced by the GDTF file and its real placement hierarchy when determining where fixtures can be mounted.
- The same resolved attachment paths are shared by Magnet guidance and fixture distribution tools.
- Truss-to-truss connections continue to use GDTF Magnet data where available.
- Added an enabled-by-default **Magnet visual feedback** preference that clearly displays compatible attachment points and lines while moving or inserting objects.

### Smarter Create from text workflow

- **Create from text** now checks unresolved fixture types before creating the scene.
- Unknown fixtures are presented in a dedicated resolution table with:
  - suggestions from the cached GDTF Share catalog,
  - manufacturer/model matching,
  - explicit GDTF mode selection,
  - manual fixture-name correction,
  - and an always-available Generic fallback.
- Confirmed mappings are remembered in the active dictionary for later riders.
- GDTF Share sign-in, catalog download, matching progress, and fixture download are integrated directly into the resolution workflow.
- A failure affecting one fixture type no longer needs to cancel the complete scene creation process.

### Improved rider parsing and placement

- Create from text now recognizes a wider range of common Spanish and English position headings for front, middle, rear, side, floor, and screen sections.
- Section detection is more conservative and no longer treats words inside equipment descriptions as position headings.
- Screens, video-control equipment, lighting-control lists, and fixture sections are separated more reliably.
- Side-lighting trusses, LED screens, rigging pipes, side-fill hoists, and explicit-length rigging entries now retain their intended dimensions, heights, and grouping.
- Filtering and parsing remain responsive with longer or heavily formatted rider text.

### MVR-xchange interoperability

- MVR-xchange TCP Mode has received a substantial interoperability and stability pass since its initial introduction in v1.5.0.
- Improved compatibility with applications including **grandMA3** and **DMXRouter**, including corrected station UUID handling for MVR requests.
- Improved station discovery, DNS-SD identity handling, JOIN/COMMIT behavior, persistent peer connections, reconnecting peers, and shutdown handling.
- Remote stations and advertised files are easier to inspect in the MVR-xchange dialog, with a dedicated station view and one-click log copying.
- The dialog layout has also been compacted to provide more space for station and file information.

### More reliable MVR import and export

- Project and standalone MVR serialization now share one canonical MVR 1.6 workflow.
- Textures, images, and external binary buffers used by 3DS and glTF geometry are preserved through MVR export and re-import.
- Fixture, truss, hoist, scene-object, hierarchy, category, color, label, mode, and identity information is preserved more consistently across project and MVR roundtrips.
- Automatic GDTF Share matching during MVR import now gives more weight to manufacturer, model, official mode, footprint, and fixture identity instead of generic descriptive text.
- MVR export now presents concise grouped fidelity warnings instead of exposing raw technical diagnostics.
- Export failures provide a focused expandable result, while harmless recovery information remains available in the log.
- Export MVR now suggests the current project name as the destination filename.

## New features and workflow improvements

- Automatic DMX patching is now topology-aware. Fixtures belonging to the same logical lighting position and physically separated bridge components are kept together whenever universe capacity allows, including deterministic serpentine ordering for parallel side structures.

- Both 2D and 3D viewports now support **Middle Mouse drag panning**.

- The 3D viewer adds independent horizontal and vertical orbit-inversion preferences that remain personal between project loads.

- Added **Selection & Movement** preferences for fixtures, trusses, supports/hoists, and scene objects. Mouse, command-bar, and Magnet transforms can independently target the exact selected object or its highest containing group without modifying the stored MVR hierarchy.

- Truss Magnet placement now prefers connector points declared by GDTF files, exposes real unoccupied connectors from truss groups, and behaves more predictably when extending straight truss assemblies.

- Magnet guidance hides truss connectors that are already occupied and keeps fixture attachment paths visible and bounded correctly in both 2D and 3D views.

- Layout PDF completion messages now report how many elements used generated Perastage symbols and how many required rendered fallbacks.

- Spanish and Simplified Chinese interface localization has been completed and hardened across dialogs and common workflows.

- Help menu commands now consistently open the intended Help, online documentation, update, log, diagnostic, and About actions.

## Compatibility, stability, and performance

- Typical projects reopen faster by loading packaged scene, configuration, and saved Layout cache data more directly.

- Heavy 2D and 3D viewports are now created only when required during startup, reducing unnecessary initialization work.

- The startup splash remains visible until the restored project window is ready, avoiding temporary pane arrangements during loading.

- Saved Layout and Legend previews are reused more effectively when their source scene has not changed, reducing unnecessary rendering after reopening a project.

- Layout 2D View elements now refresh correctly after visual scene changes, including adding, moving, editing, or removing elements and after opening or merging MVR files.

- Fixture symbol generation and caching have been extensively hardened. Replaced GDTF files are detected correctly, stale geometry is rejected, generated symbols use the selected GDTF mode and complete rendered bounds, and newly generated symbols become available immediately in Layouts, legends, print output, and PDF workflows.

- Valid external four-view fixture symbols can now be used directly, while malformed or empty symbol views are rejected safely.

- Trusses loaded from 3DS or GLB geometry now use their measured local bounds instead of fixed nominal dimensions when explicit GDTF dimensions are unavailable, improving generated dimensions and Magnet placement.

- Continuous placement and movement in the 2D and 3D viewers now remains aligned with the pointer more reliably after zooming, panning, orbiting, resizing, fitting the view, or changing standard views.

- 3D axis-constrained placement now switches axes more predictably and avoids extreme jumps when an axis is nearly aligned with the camera.

- The 3D Viewer Sketch style has improved lighting, antialiasing, edge rendering, hover feedback, and selection highlighting.

- Unicode paths and archive entry names are handled more safely across GDTF, MVR, truss dictionaries, Layout resources, and Windows filesystem workflows.

- macOS 15 Apple Silicon packaging compatibility has been restored while preserving the same fixture-symbol behavior used on Windows, Linux, and other supported macOS builds.

## Important fixes

- Fixed a project-opening stall that could leave the progress dialog at **Finalizing project load...**.

- Fixed a crash that could occur when opening another project after application startup.

- Fixed MVR-xchange TCP shutdown races and bounded protocol waits so idle or reconnecting peers do not prevent a clean shutdown.

- Fixed fixture placement in the 3D viewer so visible truss attachment paths can be acquired reliably even when perspective depth differs substantially.

- Fixed a Debug-build assertion that could occur when beginning movement with Magnet references enabled.

- Fixed truss-line fixture distribution across connected straight bridge sections and corrected millimeter/metre coordinate handling.

- Fixed distribution endpoint selection so active fixtures remain selected and endpoint markers stay aligned with the real attachment line in both 2D and 3D views.

- Fixed Bottom and Side view pointer orientation and high-DPI coordinate conversion during continuous placement.

- Fixed unconstrained 3D dragging after a Magnet preview so the object no longer inherits an incorrect snapped offset.

- Fixed the 3D viewport context menu in projects without fixtures or trusses and when viewport picking is temporarily unavailable.

- Fixed grouped scene transforms, fixture-to-hoist conversion, and table deletion so valid hierarchy ownership is preserved without dangling references or empty Undo operations.

- Fixed preservation of imported fixture IDs and unit numbers, including intentional zero values, across project save and reload.

- Fixed fixture colors in summaries and Layout legends, including recovery of valid legacy MVR colors while preserving intentionally empty user colors.

- Fixed preservation of explicit fixture categories, GDTF mode references, per-fixture visual colors, fixture label settings, hoist metadata, and truss metadata across repeated MVR/project roundtrips.

- Fixed MVR identity recovery for damaged legacy UUIDs and dependent hierarchy or hoist links.

- Fixed duplicate and case-colliding MVR archive entries so they are rejected consistently across platforms instead of producing ambiguous resources.

- Fixed MVR fixture replacement when multiple imported aliases resolve to the same GDTF Share revision and mode.

- Fixed automatic GDTF Share matching during MVR import so unrelated fixtures are no longer selected merely because they share descriptive words or similar DMX characteristics.

- Fixed Download GDTF behavior when no catalog is cached: sign-in is now requested before opening the search workflow, while an existing cached catalog remains available offline.

- Fixed fixture profiles without stored symbols or usable renderable geometry so they use the deterministic Perastage fallback without repeatedly delaying project startup.

- Fixed global shortcuts being suppressed while a read-only combo box has focus on macOS.

- Fixed PDF serialization and embedded resource handling across platforms and locale settings.

## Technical and packaging changes

- Expanded macOS Debug continuous integration to run the complete registered test suite, matching Windows and Linux coverage.

- Strengthened MVR 1.6 compliance, archive-path validation, Unicode path handling, and cross-platform regression coverage.

- Improved GitHub Actions dependency and compiler caching to make CI and installer builds more reliable without changing normal application behavior.

- Added more robust release-asset validation, build diagnostics, and cross-platform cache diagnostics.

- Improved issue-report guidance and automated maintainer triage for reports awaiting feedback.

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
