# Perastage v1.4.0 Release Notes

Changes since **v1.3.0**.

Perastage v1.4.0 focuses on faster scene building, cleaner rigging workflows, and more reliable MVR/GDTF exchange. This release adds pointer-driven placement for fixtures, trusses and scene objects, new 2D/3D movement controls, Group and Ungroup workflows, extended primitive geometry tools, better diagnostics, and many compatibility fixes for imported and exported MVR projects.

## Highlights

- New continuous placement workflow for fixtures, trusses and scene objects.
- New Drag-Move, Axis Lock and Magnet controls in the 2D/3D viewers.
- New Group and Ungroup commands based on an MVR-compatible GroupObject hierarchy.
- Improved truss insertion, bridge creation and truss geometry export options.
- Clearer fixture color handling, with separate visual Type Color and MVR Color Filter fields.
- More robust MVR/GDTF export, including canonical GDTF naming and stricter MVR 1.6 compliance.
- Better diagnostics, crash reports and release symbol archives for troubleshooting.
- Updated built-in fixture, truss and scene-object library content.

## New features

- **Continuous placement** - Add Fixture, Add Truss and Add Object can now place a series of items directly in the visible 2D or 3D viewer. Left-click confirms each item, while right-click or Escape cancels the pending placement.
- **Drag-Move toolbar toggle** - enable left-click dragging of selected items without changing the normal pan/navigation behavior when the tool is disabled. The setting is saved and remains disabled by default.
- **Axis Lock toggle** - keep movement constrained to scene axes by default, or disable it for freer 2D movement and view-plane movement in 3D.
- **Magnet snapping** - optionally snap fixtures and objects to trusses while moving them in 2D/3D. Snapping is disabled by default and uses the active viewer direction to choose better snap targets.
- **Group and Ungroup commands** - group mixed selections from the scene tables and viewers using Edit > Group or `Ctrl+G`, and ungroup them with Edit > Ungroup or `Ctrl+U`.
- **Truss import/export preferences** - choose how truss geometry is handled when importing and exporting MVR files, including standard MVR output and Geometry3D compatibility workflows.
- **Improved Add Truss workflow** - place trusses at real-world X/Y/Z coordinates, create multiple trusses along a line, and optionally group them as a bridge.
- **Extended primitive geometry** - create and edit spheres, cubes and cylinders with custom names, persistent default sizes, position/rotation controls and reusable templates.
- **Diagnostics from the Help menu** - open the local logs folder or export a manual diagnostic report when a user needs to report a problem.
- **Update notification control** - avoid repeated startup prompts for the same available version while keeping manual update checks available.

## Improvements

### Scene building and movement

- Fixtures, trusses and scene objects now share a more consistent placement workflow.
- Movement feedback is clearer, with 2D/3D gizmos and highlighted X/Y/Z status readouts during drags.
- Group selection is easier to understand: selecting or hovering a grouped item highlights the full root group, including nested groups.
- Individual fixtures can still be moved inside grouped truss workflows without unintentionally dragging the whole group.
- The 3D move gizmo now has clearer Blender-style cone arrowheads.
- Toolbar disabled states are clearer in dark mode for selection, measurement, axis lock, drag-move and magnet tools.
- `Ctrl` and `Ctrl+Shift` selection in the 2D/3D viewers is now additive instead of clearing the current selection.

### Magnet snapping

- Fixture-to-truss snapping now uses actual truss bounds and edge height instead of simple center or bounding-box guesses.
- Snapped fixtures remain aligned when the target truss or truss group moves.
- Snapping starts only after an intentional drag and releases based on the raw pointer position.
- Grouped truss runs snap using their exterior bounds and ignore attached fixtures when matching to loose trusses.
- Magnet, Axis Lock and Drag-Move states stay synchronized when opening projects, switching layouts or changing active viewers.

### Fixture color and editing

- Fixture color handling is now clearer:
  - **Type Color** controls the Perastage visual color used in plans, legends, summaries and type-based coloring.
  - **Color Filter** controls the standards-based MVR `Fixture/Color` value.
- Existing dictionaries that use `color` still load correctly and are saved with the clearer `visual_color` key where appropriate.
- Type colors, including automatically assigned colors, are preserved through Perastage MVR metadata when projects are reopened.
- Editing fixture Weight and Power Consumption updates shared GDTF type data for matching fixtures.
- Fixture table references are refreshed after Perastage promotes a modified fixture to a derivative GDTF.

### Rigging and hoists

- Automatic hoist loads are calculated and displayed when the hoist table opens.
- The Load editor can insert the calculated value into the input for review before confirming or cancelling.
- Manual load overrides are preserved as Perastage metadata and highlighted in the truss table.
- Values that match the automatic calculation remain automatic instead of being saved as manual overrides.
- Tooltips explain manual values and missing-weight calculations more clearly.
- The redundant hoist Data Source column has been removed.

### MVR/GDTF exchange

- Exported MVR files identify the current Perastage version.
- Generated and Perastage-modified GDTF files use a canonical `Manufacturer@FixtureType@Perastage.gdtf` naming convention where appropriate.
- Imported MVR projects keep embedded GDTF resources synchronized during save/export, even when canonical naming changes the final archive names.
- Perastage-generated fixture GDTFs are copied into the MVR resource root before references are updated.
- MVR export resources are written at the archive root without exposing local filesystem paths.
- MVR 1.6 output now follows stricter child ordering for fixtures, trusses, supports and scene objects.
- Perastage-specific fixture, truss, support, hoist, primitive and layer metadata is stored in root-level Perastage UserData blocks where required for cleaner standards compliance.
- Truss symbols preserve stable UUIDs and proper Symdef references.
- Canonical Truss UUIDs are written for MVR 1.6 files.
- Support objects are preserved during round-trip MVR export/import, including placeholder support data where required.
- SceneObject primitive metadata round-trips through root-level Perastage metadata while keeping SceneObject children in specification order.
- Fixture UnitNumber values are preserved when present or generated deterministically by fixture type and position when missing.
- Fixture Weight and Power Consumption are exported through standard GDTF PhysicalDescriptions instead of non-standard MVR children.
- Generated truss GDTFs use stable valid fixture type UUIDs and specification-ordered sections.
- Standalone and MVR-embedded GDTFs now share the same canonical export layer, reducing legacy Perastage-only audit nodes in exported files.

### Geometry, rendering and model handling

- Sketch mode now uses the same corrected mesh-normal path as standard rendering, avoiding isolated models that appear lit from the wrong side.
- GDTF fixtures that use `GeometryReference` now render geometry only at the referenced hierarchy position.
- Fixture-specific DMX modes select the correct geometry root without sharing incompatible cached geometry.
- Cube primitive normals now face outward correctly on all faces.
- Hollow or U-shaped meshes are selected by visible geometry rather than by their bounding boxes.
- GLB and 3DS truss models remain visible and selectable when metadata is incomplete, with clear rejection of unsupported formats.
- Native 3DS mesh dimensions are preserved more reliably.
- Rotated truss fallback sizing is more accurate.
- 3D truss height labels now include the selected project distance unit suffix.

### Libraries and Create from text

- Built-in fixture, truss and scene-object libraries have been reviewed and adjusted for better consistency.
- New scene-object library resources have been added.
- Create from text now normalizes hoist/motor preview lines using the English `FOR` keyword.
- Rider import is faster thanks to cached truss definitions, cached GDTF metadata, reused cleanup expressions and fewer repeated filtering passes.

### Command bar and tables

- Transform commands now support `t` and `thru` separators for position and rotation values.
- Fixture, scene-object, truss, hoist, rigging, layer, summary and dictionary tables use named column identities and safer index validation internally.
- Truss table reloads preserve selection more reliably and avoid side effects during refresh.
- Scene object names edited in the Data View now persist across save/reopen and are exported through MVR.
- Fixture IDs remain as edited when reopening or exporting projects.

## Fixes

This release includes many fixes across importing, editing, movement and export workflows, including:

- Fixed imported MVR projects losing embedded GDTF resources during save/export after canonical GDTF renaming.
- Fixed project saves rejecting or replacing empty `scene.mvr` payloads more safely, with clearer logging and save-failure messages.
- Fixed fallback dummy GDTF synchronization when imported project resources are missing or renamed.
- Fixed GroupObject layer ownership so fixtures added to truss-based groups adopt the truss/group layer while existing trusses and groups keep their original layer across save, reopen, import and export.
- Fixed Add Truss listing every placed instance instead of each available truss type once.
- Fixed Add Truss reusing extracted symbol resources instead of the original GDTF, GTruss, GLB or 3DS definition.
- Fixed continuous fixture placement offsets after confirming a Magnet-snapped fixture.
- Fixed placement behavior after viewer navigation, Undo, Magnet changes and axis-lock changes.
- Fixed rapid Magnet snap/release flicker near truss edges during pointer-follow placement.
- Fixed visual color rename build issues in fixture editing and fixture replacement.
- Fixed fixture color export so fixture visual colors and MVR color/filter values are not confused.
- Fixed layer color metadata export/import using PerastageLayerAppearance metadata.
- Fixed 2D/3D quick-click selection for grouped items and fixtures.
- Fixed hover highlights staying pinned to dragged elements.
- Fixed fixture edge rendering and hover responsiveness in the 3D viewer.
- Fixed scene-object mesh picking for hollow and U-shaped objects.
- Fixed command-bar `t` and `thru` parsing for transform ranges.
- Fixed MVR Symbol UUID export.
- Fixed support round-trip export/import and truss child ordering.
- Fixed MVR SceneObject identity determinism.
- Fixed GDTF output ordering and revision entries for safe structural export changes.
- Fixed macOS, Linux and Windows build issues related to filesystem paths, timestamps, compiler headers and installer generation.
- Fixed Linux UTF-8 and GTK rich-text toolbar warnings.
- Fixed OpenGL initialization checks so 2D/3D viewers can skip rendering safely when a context is not available.

## Stability and diagnostics

- Perastage now initializes a UTF-8 process text locale at startup.
- Local diagnostic logging records useful build, platform, wxWidgets, OpenGL and runtime context.
- Crash handling has been added with stack-trace support where available.
- Application shutdown no longer waits unnecessarily on pending log messages.
- Truss archive extraction rejects unsafe ZIP entries.
- 3D resource lookup avoids unbounded fallback scans and handles filesystem errors more gracefully.
- Malformed MVR or GDTF resources no longer stop the scene from synchronizing into the viewport.
- Texture loading avoids duplicate image-handler registration.
- The 2D viewer validates GLEW initialization before enabling OpenGL.
- 2D and 3D viewers skip selection/rendering safely when the OpenGL context is unavailable.

## Build, packaging and CI

- Added backward-cpp integration for improved diagnostics.
- Added build metadata used by diagnostics and release outputs.
- Added CI symbol archives to make crash reports easier to investigate after a release.
- Improved Linux, macOS, Windows and Arch Linux build reliability.
- Improved vcpkg cache handling and compatibility with newer macOS/Xcode environments.
- Improved Windows installer generation and Visual Studio generator detection.

## Documentation

- Added documentation for basic geometry primitives and editing controls.
- Updated preferences documentation for MVR import/export behavior.
- Updated MVR/GDTF export and mutation policy documentation.
- Added troubleshooting guidance for diagnostics and release-symbol assets.
- Updated shortcut and command-bar documentation.

## Compatibility notes

- Existing projects should continue to load normally.
- Drag-Move and Magnet are optional tools and remain disabled by default.
- Axis Lock remains enabled by default to keep movement predictable.
- Perastage may create derivative GDTF files when it needs to modify or canonicalize a fixture resource; original library assets are preserved by default.
- Some export changes are intentionally focused on stricter MVR/GDTF compliance and cleaner round-trip behavior with other tools.
