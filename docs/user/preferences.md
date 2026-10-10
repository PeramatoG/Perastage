# Preferences

Perastage includes a Preferences dialog for user-level behavior and display settings.

## Common settings

Current user-facing preferences include:

- Distance units
- Weight units
- GDTF-related settings
- Update check behavior (startup recommended or manual only)
- MVR import/export settings
- 3D model detail and an optional navigation proxy

## Why preferences matter

- Unit settings affect how values are shown in the interface.
- GDTF-related options affect fixture profile handling and lookup behavior.
- Preferences help keep the app aligned with your workflow without changing scene data intent.

## GDTF definition completion

The **GDTF** tab offers two policies under **GDTF definition completion**:

- **Complete and improve GDTF definitions (recommended)** is the default.
  Perastage may generate missing standard Top, Side, and Front SVG views from
  available geometry and publish an audited `@Perastage.gdtf` derivative.
  Existing views are preserved, including unusable views that need explicit repair.
- **Preserve imported GDTF definitions** disables automatic completion.

Both policies keep normal scene editing, explicit GDTF edits, and Perastage
project symbols available. Manually applying Perastage views stores a project
override in `.pstg`; it does not replace authored GDTF views. Save the project
to preserve generated symbols and overrides, including symbols applied before
the scene has first been saved.

## MVR Import / Export

The **MVR Import / Export** tab is the location for MVR-related import and export preferences. The current export setting controls how truss geometry is written when exporting MVR files.

### Truss geometry export mode

- **Standard MVR representation**: the default. Perastage preserves imported Truss `Symbol`/`Symdef` references when possible, keeping the standards-oriented structure intact.
- **Direct Geometry3D for truss symbols**: Perastage expands truss `Symbol`/`Symdef` references into direct `Geometry3D` entries under the exported Truss node. This can improve compatibility with importers that expect direct truss geometry while still writing valid MVR.

Project saves keep the standard representation so internal `.pstg` scene storage remains non-lossy.

## Selection & Movement

The **Selection & Movement** page controls which object types move their entire
containing group during interactive transformations. By default, grouped
trusses move their group, while fixtures, supports/hoists, and scene objects
move independently. Each type can be configured separately.

This policy applies consistently to mouse interaction in Viewer2D and Viewer3D,
command-bar position and rotation commands, and Magnet preview and commit. When
promotion is enabled, Perastage uses the highest parent `GroupObject`, not only
the immediate parent. Directly selecting a `GroupObject` always moves it as a
group.

Table edits always modify only the edited row's exact object. Changing these
user preferences does not alter grouping, transforms, `.pstg` project data, or
MVR hierarchy and does not mark the project dirty.

### Magnet visual feedback

**Show anchor references while moving or inserting elements** is enabled by
default. When Magnet is enabled and an element is being moved or inserted, the
2D and 3D viewers display every compatible anchor as a vivid red point or
direction line. The references remain visible throughout the interaction,
disappear when it ends, and do not become part of the scene or exported data.

## Update check behavior

- **Check on startup (recommended)**: checks on launch, limited to at most once every 24 hours. When a newer version is found, you can choose **Do not remind me again for this version** to suppress future startup reminders for that same version.
- **Manual only**: does not run startup checks; use **Help → Check for Updates** on demand. Manual checks still show the current result even if a startup reminder was previously dismissed.

## Good practice

After changing preferences, recheck your scene in both 2D and 3D and verify table values display as expected.

## 3D Viewer model detail

Under **3D Viewer > Performance**, **Model detail** controls the runtime
representation of fixture, truss, and scene-object meshes:

- **Standard** (default) balances detail and performance. Small meshes stay complete;
  heavy meshes use a conservative simplification.
- **Low** requests a smaller representation for heavy meshes.
- **High** renders all available authored mesh detail.

**Use a simplified proxy while navigating** is a separate option, disabled by
default. When enabled, unselected fixtures may temporarily use a more aggressive
proxy during fast navigation, even with High selected. When navigation ends,
the chosen model detail returns. Caching, GPU buffers, and instancing remain
available with every detail level and with the proxy disabled.

Apply and OK take effect on the next redraw without reopening the project.
These are Perastage rendering preferences: source meshes and GDTF resources
remain unchanged. 2D views, symbol captures, and exported geometry continue to
use the complete geometry. Detail levels are not inferred from GDTF filenames
or from multiple authored models.
