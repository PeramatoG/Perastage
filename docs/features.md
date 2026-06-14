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
- Parametric objects exported as Fixture/Truss/Support receive stable non-empty `FixtureID` and globally unique `FixtureIDNumeric` values.

### GDTF integration and dictionary pipeline

- Local fixture dictionary maps textual fixture descriptions to GDTF specs under `library/`.
- GDTF-Share download flow is available from the Tools menu.
- Export packages GDTFs under `gdtf/` with archive-relative forward-slash paths.
- Filename collisions are handled deterministically (`name (1).gdtf`, etc.) and references are updated.
- Policy details for mutation, revisions, and schema fallback are maintained in [GDTF mutation policy](gdtf_mutation_policy.md).

## Rider and Text-to-Scene Generation

### Inputs and parsing flow

- Rider-like imports accept text and PDF input through **Tools → Create from text**.
- **Apply filter** supports parser-first cleanup before final object creation.
- Parser understands hang tokens, including optional coordinate payloads where supported.
- `CALLES` and `SIDES` headers map into side-truss/fixture placement workflows.

### Rules contract

- Parsing and placement behavior is governed by [Text-to-scene rules](text_to_scene_rules.md).
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
- The Magnet toolbar toggle is disabled by default and can be enabled to snap dragged trusses, fixtures, and scene objects to nearby compatible scene bounds in the 2D and 3D viewers. Fixture snaps use the nearest truss bounding-box surface or edge even when fixtures and trusses are managed from different tables. Truss-to-truss snaps can create or extend an official GroupObject when the mouse is released, while fixture and scene-object snaps only update transforms. Magnet does not modify Hang Position, does not merge geometry, and keeps exported MVR data standards-compliant.
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
- Detailed precedence and scope are tracked in [GUI shortcut architecture](gui_shortcut_architecture.md).

## Future/Experimental

- Some tools remain Debug-only for production safety in Release builds.
- Large-scene performance and selected advanced workflows continue to be iterated.

## Related Documents

- [Changes since beta 0.1.0](changes_since_beta_0_1_0.md)
- [Build and dependency guide](build.md)
- [Packaging and platform integration](packaging.md)
- [Build troubleshooting](troubleshooting.md)
- [Documentation policy and synchronization checklist](documentation_policy.md#documentation-synchronization-checklist)
