# Perastage Feature Overview

This page summarizes the current user-facing workflows in Perastage.

## Core workflows

- Open and review stage scenes from `.mvr`.
- Create and manage Perastage projects (`.pstg`).
- Inspect and edit scene data through synchronized 3D/2D viewers and data tables.
- Produce layout/plan documentation for print/export.

## Scene data and editing

Perastage provides dedicated data workflows for:

- **Fixtures**
- **Trusses**
- **Hoists**
- **Scene objects** (including primitive geometry)
- **Layers** and layer-based organization

Additional production helpers include patching, summary/rigging overviews, and console-based selection/transform commands.

## MVR and GDTF

- Import `.mvr` scenes for review and editing.
- Export scene data back to `.mvr`.
- Use local GDTF dictionaries for fixture mapping.
- Optionally use GDTF Share credentials/download workflows when configured in the app.

## Text-to-scene

From **Tools > Create from text...** you can:

- normalize rider text with **Apply filter**,
- create fixtures, trusses, and scene objects from parsed text,
- use supported keywords such as `truss`, `pipe`, `pipes`, `vara`, `varas`.

Full parser behavior is documented in [Text-to-scene rules](text_to_scene_rules.md).

## Basic geometry

From **Edit > Add basic geometry** you can create:

- `Sphere`
- `Cube`
- `Cylinder`

New primitives are created on the active layer.

## Visualization and layout output

- **3D Viewer** for scene navigation and review.
- **2D Viewer** for plan-oriented navigation.
- **Layouts** for page composition (views, tables, legends, text, images).
- Export/print flows for technical documentation.

## Related docs

- [Perastage Help](../help.md)
- [Text-to-scene rules](text_to_scene_rules.md)
- [GUI shortcut architecture](gui_shortcut_architecture.md)
- [Packaging](packaging.md)
