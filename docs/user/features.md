# Perastage Feature Overview

This document is the canonical feature map for the current Perastage release line. It consolidates the active workflows that have evolved from the beta period into the current production-oriented toolset.

## Audience and Primary Use Cases

Perastage is designed for lighting designers, programmers, and technicians who need to:

- import and normalize real show data,
- visualize rigs in 3D and 2D,
- maintain fixture/truss/hoist/object metadata,
- generate printable and shareable technical documentation.

## Project and Scene Lifecycle

### Project files and defaults

- Perastage projects use the `.pstg` format.
- New projects can load default layout templates from `library/default_layouts/`.
- Save, Save As, and Load workflows preserve scene and user-facing project context.

### Layer-aware organization

- Fixtures, trusses, hoists, and objects can be grouped into layers.
- Layer visibility and selection behavior is shared across viewer and documentation workflows.
- Active layer controls where newly created items are placed.

## MVR and GDTF Workflows

### MVR import and open behavior

- Imports MVR 1.6 scenes with fixtures, trusses, hoists/supports, and generic objects.
- `.mvr` opening from startup, command-line, or OS association uses a clean-scene reset plus import flow for deterministic behavior.
- Menu import lets you choose between opening the selected MVR as a new project or merging it into the current project.
- Merged MVR content preserves existing scene content, resolves imported UUID collisions so both scenes can coexist, prompts before fixture type/GDTF definition conflicts are applied, and reports duplicate DMX patch addresses as non-blocking warnings for post-merge review.

### MVR export

- Exports the current scene back to `.mvr` for interoperability.
- Keeps **Type Color** separate from **Color Filter**: Type Color is a
  Perastage visual used by plans, summaries, legends, and type-based viewer
  coloring and is shared by fixture type and mode, while Color Filter is the
  optional per-fixture color imported from and exported to the MVR
  `Fixture/Color` node.
- Stores Type Color by fixture profile and mode in root-level Perastage
  `UserData`, allowing table swatches, summaries, legends, and type-based
  viewer colors to survive project save and reopen without using the standard
  MVR `Fixture/Color` node.
- Stores Perastage-specific layer appearance metadata, including layer colors, in root-level `UserData/Data` with Perastage-owned `PerastageLayerAppearance` entries instead of writing non-standard color data inside standard `Layer` nodes.
- Parametric objects exported as Fixture/Truss/Support receive stable non-empty `FixtureID` and globally unique `FixtureIDNumeric` values.

### GDTF integration and dictionary pipeline

- Local fixture dictionary maps textual fixture descriptions to GDTF specs and
  Perastage visual-color defaults under `library/`. Dictionary files now write
  the explicit `visual_color` key while continuing to read the legacy `color`
  key.
- GDTF-Share download flow is available from the Tools menu.
- Export packages GDTFs under `gdtf/` with archive-relative forward-slash paths.
- Filename collisions are handled deterministically (`name (1).gdtf`, etc.) and references are updated.
- Policy details for mutation, revisions, and schema fallback are maintained in [GDTF mutation policy](../developer/gdtf_mutation_policy.md).

## Rider and Text-to-Scene Generation

### Inputs and parsing flow

- Rider-like imports accept text and PDF input through **Tools → Create from text**.
- **Apply filter** supports parser-first cleanup before final object creation.
- Parser understands hang tokens, including optional coordinate payloads where supported.
- `CALLES` and `SIDES` headers map into side-truss/fixture placement workflows.

### Rules contract

- Parsing and placement behavior is governed by [Text-to-scene rules](../developer/text_to_scene_rules.md).
- Any parser behavior changes must update the rules document in the same change set.

## Dictionary Portability and Asset Handling

Perastage dictionary import/export supports multiple transport levels:

- JSON snapshot (references only),
- JSON snapshot with optional copied assets,
- portable ZIP bundle with manifest and assets.

Additional safeguards include:

- preflight path validation,
- missing-reference reporting,
- collision policy prompts for differing file content.

## Patch, Data Tables, and Editing Helpers

### Patch management

- Manual and assisted patch workflows support universe and address management.
- Auto patch can group by hang position/type and assign channels sequentially.

### Data tables

- Dedicated tables for fixtures, trusses, hoists, and objects.
- Fixture, truss, and hoist Hang Position cells use the shared Hang Position dialog, which lists existing MVR positions and can add, rename, or delete positions across affected rigging items when confirmed.
- Add Fixture, Add Truss, and Add Object offer a continuous placement mode that
  disables the fixed quantity field and attaches one new element at a time to
  the pointer in the visible 2D or 3D viewer. Left-click confirms each element
  and immediately starts the next copy; right-click or Escape cancels only the
  pending copy.
  Magnet snapping and axis-constrained movement follow the current viewer
  toolbar settings, including changes made while placement is active. Holding
  the left button and dragging navigates the active viewer instead of
  confirming an element; releasing the button immediately realigns the pending
  element beneath the pointer. Undo removes the most recently confirmed element
  while leaving a provisional copy attached to the pointer. Undoing the first
  confirmed element, or undoing before any confirmation, cancels continuous
  placement.
- Add Truss supports quantity, real-world insertion-point coordinates in the active distance units, automatic linear placement from the selected truss bounding-box length, and optional default grouping into one bridge.
- **Tools → Convert Scene Objects to Truss** converts the selected scene object and every other scene object that uses the same model file into truss objects. The same command is available by right-clicking a scene object in the 2D or 3D viewer.
- Multi-row editing helpers include fills, ranges, interpolation, and relative expressions.
- **Group** and **Ungroup** in the Edit menu create and remove MVR-compatible GroupObject hierarchy from the active cross-table selection across fixtures, trusses, hoists, and scene objects while preserving world placement and hang-position assignments; after grouping, clicking a member selects the full group across related tables, and the selection highlights and moves/rotates the full group, including nested groups, while hovering a grouped member uses a more yellow primary highlight and a paler related-green highlight on the other group members in the 3D view and related tables using the same row style as table selection.
- CSV export is available through file/export workflows.

### Conversion and type/color helpers

- **Auto color** can assign colors by layer/type while preserving explicit colors.
- **Convert to Hoist** transforms selected fixtures into supports while retaining scene context.
- **Replace fixtures** (Edit menu) swaps selected fixtures to a chosen fixture source (scene fixture, dictionary fixture, or GDTF file) while preserving placement and patch identity fields.

## Visualization and Layout Production

### 3D Viewer

- OpenGL-based viewer with orbit, pan, zoom, and preset camera views.
- Selection flows integrate with scene tables and command operations.
- The viewport toolbar includes a Drag Move toggle for moving scene selections with a left-click drag. It is disabled by default to make viewport panning safer in dense scenes, and its state is stored with the project.
- The viewport toolbar includes an axis-lock toggle for dragged scene selections. It is enabled by default for axis-constrained moves; disabling it stores the project setting and allows Blender-style free dragging on a plane parallel to the 3D camera view.
- Context menu includes **Render style** with these options:
  - **Standard** for general-purpose scene reading.
  - **Sketch mode** for high-readability geometry outlines.
  - **Textured** for material-aware scenic validation.
  - **Wireframe** for technical debugging and overlap checks.
  - **White** for neutral shape review.
  - **By device type** for fast fixture-category grouping.
  - **By layer** for layer-organization validation.
  - **By universe** for DMX universe distribution checks.

### 2D Viewer

- Top-down plan visualization with configurable grid and labels.
- The shared viewport axis-lock toggle also controls 2D selection dragging: enabled keeps movement constrained to one screen axis, while disabled allows free movement across both axes in the active 2D plane.
- The Magnet toolbar toggle is disabled by default and can be enabled to snap dragged trusses, fixtures, and scene objects to nearby compatible scene bounds in the 2D and 3D viewers. Fixture snaps use the nearest truss bounding-box surface or edge even when fixtures and trusses are managed from different tables. Truss-to-truss snaps can create or extend an official GroupObject when the mouse is released, and fixture-to-truss snaps add the fixture to the truss snap group so it can later be detached without flattening the rest of the group. Scene-object snaps only update transforms. Magnet does not modify Hang Position, does not merge geometry, and keeps exported MVR data standards-compliant.
- The Cross-table Actions toolbar toggle is disabled by default and can be enabled to make viewport hover, selection, measuring, and compatible interaction tools work across fixtures, trusses, hoists, and scene objects regardless of the currently selected Data Views table.
- Supports vector draw-command capture for downstream document/export workflows.

### Layout system

- Multi-page layout authoring with page naming and orientation control.
- Place and edit views, legends, event tables, text blocks, and image elements.
- Layouts can be exported and printed as production documentation sheets.

## Printing and Export Outputs

- Layout-to-PDF output for full documentation sets.
- Print table workflows for fixtures/trusses/hoists/objects.
- Debug-only 2D direct print dialog is gated in Release builds via feature flags.

## GUI Workflow and Operations

### Console and command workflows

- Console supports text command workflows for selection and transform operations.
- Command history and prompt behavior provide fast operator iteration in large scenes.

### Units and preferences

- Distance and weight systems are independently configurable.
- Internal canonical values remain millimeters (distance) and kilograms (weight).
- Input parsing accepts explicit unit suffixes regardless of active display system.

### Shortcut and interaction model

- Keyboard/mouse interaction includes viewer navigation, selection, and layout editing actions.
- Detailed precedence and scope are tracked in [GUI shortcut architecture](../developer/gui_shortcut_architecture.md).

## Future/Experimental

- Some tools remain Debug-only for production safety in Release builds.
- Large-scene performance and selected advanced workflows continue to be iterated.

## Related Documents

- [Changes since beta 0.1.0]()
- [Build and dependency guide](../developer/build.md)
- [Packaging and platform integration](../developer/packaging.md)
- [Build troubleshooting](troubleshooting.md)
- [Documentation policy and synchronization checklist](../developer/documentation_policy.md#documentation-synchronization-checklist)
