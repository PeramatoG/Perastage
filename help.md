# Perastage Help

Perastage is a high-performance, cross-platform viewer for MVR (My Virtual Rig) scenes with 3D rendering, scene analysis and editable tabular data interfaces. It is designed for lighting professionals and developers working with the GDTF and MVR standards.

## Getting Started

1. Launch the application.
2. Use **File → Import MVR** to load an `.mvr` file.
3. Navigate through the 3D viewport and explore scene elements via:
   - Fixtures Table
   - Trusses Table
   - Objects Table
4. Use the **View** menu to toggle visibility of side panels and tables.

## Keyboard Shortcuts

### Global

| Key Combination | Function             |
|-----------------|----------------------|
| Ctrl+N          | New project          |
| Ctrl+L          | Load project         |
| Ctrl+S          | Save project         |
| Ctrl+Q          | Close application    |
| Ctrl+Z / Ctrl+Y | Undo / Redo          |
| Del             | Delete selection     |
| F1              | Open help            |
| 1 / 2 / 3       | Switch between tables|

### 3D Viewer

| Key Combination    | Function                 |
|--------------------|--------------------------|
| Arrow Keys         | Orbit camera             |
| Shift + Arrow Keys | Pan camera               |
| Alt + Arrow Keys   | Zoom in/out              |
| Numpad 1 / 3 / 7   | Front, Right, Top views  |
| Numpad 5           | Reset camera orientation |

### 2D Viewer

| Key Combination  | Function    |
|------------------|-------------|
| Mouse drag       | Pan view    |
| Mouse wheel      | Zoom in/out |
| Arrow Keys       | Pan view    |
| Alt + Arrow Keys | Zoom in/out |

## Panels and Dialogs

### Fixture Table
- Displays all parsed fixture objects from the MVR file.
- Columns include: Name, Fixture Type, Mode, Address, Layer, Position.
- Rotation columns are labeled **Roll (X)**, **Pitch (Y)** and **Yaw (Z)** and
  are applied in Z/Y/X order.
- Context menu options:
  - Edit Address (via `AddressDialog`)
  - Replace Fixture (via `AddFixtureDialog`)
  - Export to GDTF (`ExportFixtureDialog`)

### Truss Table
- Displays imported trusses with geometry, dimensions, and metadata.
- Rotation columns are labeled **Roll (X)**, **Pitch (Y)** and **Yaw (Z)**.
- Export functionality provided by `ExportTrussDialog`.

### Objects Table
- Lists generic scene objects and their transforms.
- Rotation columns are labeled **Roll (X)**, **Pitch (Y)** and **Yaw (Z)**.

### Console Panel
- Displays status messages and logs.
- Can be toggled from the View menu.

### Layer Panel
- Lists scene layers and lets you choose the active layer for new objects.

### Summary Panel
- Provides counts and basic statistics for fixtures, trusses and objects.

## File Menu Options

| Menu Option   | Description                                |
|---------------|--------------------------------------------|
| New           | Start a new project (`Ctrl+N`)             |
| Load          | Open an existing project (`Ctrl+L`)        |
| Save          | Save the current project (`Ctrl+S`)        |
| Save As       | Save project under a new name              |
| Import Rider  | Load fixture/truss info from `.txt`/`.pdf` |
| Import MVR    | Load an `.mvr` scene file                  |
| Export MVR    | Write current scene to MVR                 |
| Print Table   | Print one of the data tables               |
| Export CSV    | Export table data to CSV                   |
| Recent Files  | Access previously opened scenes            |
| Close         | Close the application (`Ctrl+Q`)           |

## Edit Menu

| Menu Option        | Description                                |
|--------------------|--------------------------------------------|
| Undo               | Revert last change (`Ctrl+Z`)               |
| Redo               | Reapply last change (`Ctrl+Y`)              |
| Add fixture        | Insert a new fixture                        |
| Add truss          | Insert a new truss                          |
| Add scene object   | Insert a generic scene object               |
| Delete             | Remove selected items (`Del`)              |
| Preferences        | Open configuration dialog                   |

## View Menu

| Menu Option        | Description                                  |
|--------------------|----------------------------------------------|
| Console            | Show/hide console output                     |
| Fixtures           | Toggle table notebook (Fixtures/Trusses/Objects) |
| 3D Viewport        | Enable or disable 3D rendering               |
| 2D Viewport        | Enable or disable top-down view              |
| 2D Render Options  | Show/hide render options for 2D view          |
| Layers             | Toggle layer panel                           |
| Summary            | Toggle summary panel                         |
| Layout → Default   | Restore default panel layout                 |
| Layout → 2D        | Activate preset 2D layout                    |

## Tools Menu

| Menu Option           | Description                                   |
|-----------------------|-----------------------------------------------|
| Download GDTF fixture | Fetch a fixture from GDTF‑Share               |
| Export Fixture        | Export selected fixtures to GDTF              |
| Export Truss          | Export selected trusses                       |
| Export Scene Object   | Export selected scene object model            |
| Auto patch            | Assign addresses automatically to fixtures   |
| Auto color            | Assign random colors to fixtures and layers  |

## Console Commands

The Console panel provides a lightweight command-line interface for precise
editing. Commands operate on the current selection of fixtures or trusses.

### Selection

- `f <ids>` – select fixture IDs. Use `+` or `-` to add or remove IDs and pairs
  of numbers to define ranges.
- `t <ids>` – select truss IDs.
- `clear` – clear all selections.

Examples:

```
f 1 4             # select fixtures 1 through 4
f + 6 8           # add fixtures 6 through 8
f - 2             # remove fixture 2
t 1 3             # select trusses 1 through 3
clear             # clear selection
```

### Position and Rotation

- `pos x 1.5` – set the X position of selected items to 1.5 m.
- `pos 0 0 5` – set X/Y/Z directly.
- `x ++0.5` – move items 0.5 m along X (use `--` to subtract).
- `rot z 90` – set yaw to 90°.
- `rot ++45` – add 45° to the current rotation on all axes.

Values are in meters for position and degrees for rotation.

## Example Workflow

1. Import an `.mvr` scene via **File → Import MVR**.
2. Press **1** to show the Fixtures table and select fixtures.
3. Open the Console panel and run `f 1 4` followed by `pos z ++1` to raise
   fixtures 1–4 by one meter.
4. Choose **Tools → Auto patch** to assign DMX addresses.
5. Choose **Tools → Auto color** to apply random colors.
6. Enable **View → Summary** to review fixture counts.
7. Export data with **File → Export CSV**.

## Configuration Management

Settings are stored using a modular configuration manager. It supports saving and restoring:

- Panel visibility states
- Last used file paths
- View mode preferences

## File Format Support

- MVR 1.6: Import of scenes, positions, trusses, fixtures
- GDTF: Supports embedded fixture definitions
- 3DS / GLB: 3D model formats used in MVR/GDTF

## Planned and Experimental Features

- Grouping and filtering by Layer
- 2D plan generation
- Automatic basic layout generation from text
- Enhanced GDTF file support
- Dictionary for replacing autogenerated basic GDTF files with detailed versions

## Developer Information

- Organized codebase:
  - `core/`: logic and data handling
  - `gui/`: user interface components
  - `render/`: future rendering backend
- Developed with C++20 and CMake
- Uses `wxWidgets`, `tinyxml2`, and other open libraries

## Troubleshooting

- If the 3D viewport does not display:
  - Confirm OpenGL libraries are installed
  - Ensure the MVR contains valid geometry and GDTF references
- If objects do not appear in tables:
  - Verify the MVR contains proper object definitions
  - Check the console for warnings or errors

## Additional Documentation

- MVR Specification: `docs/mvr-spec.md`
- GDTF Specification: `docs/gdtf-spec.md`
- License: `LICENSE.txt` (GPL v3)
