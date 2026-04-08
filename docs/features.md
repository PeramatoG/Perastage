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
- `.mvr` opening uses a clean-scene reset plus import flow for deterministic behavior.
- Menu import and OS association entry points use the same progress and UI lock strategy.

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
- CSV export is available through file/export workflows.

### Conversion and type/color helpers

- **Auto color** can assign colors by layer/type while preserving explicit colors.
- **Convert to Hoist** transforms selected fixtures into supports while retaining scene context.

## Visualization and Layout Production

### 3D Viewer

- OpenGL-based viewer with orbit, pan, zoom, and preset camera views.
- Selection flows integrate with scene tables and command operations.
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
- **Peraviz (including DMX/Art-Net integration) is experimental and in an early phase.**
- **Peraviz is not part of the recommended main Perastage production workflow today.**
- For teams evaluating this area, see:
  - [Peraviz DMX Input (Art-Net RX)](DMX_ARTNET.md) *(experimental maturity; subject to change)*.
  - [DMX patch from MVR + Dimmer mapping from GDTF](DMX_PATCH_MVR_GDTF.md) *(experimental maturity; subject to change)*.
- Large-scene performance and selected advanced workflows continue to be iterated.

## Related Documents

- [Changes since beta 0.1.0](changes_since_beta_0_1_0.md)
- [Build and dependency guide](build.md)
- [Packaging and platform integration](packaging.md)
- [Build troubleshooting](troubleshooting.md)
- [Documentation policy and synchronization checklist](documentation_policy.md#documentation-synchronization-checklist)
