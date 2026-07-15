# Create from Text

Use **Tools → Create from text** to build scene content from rider-style text.

## Supported inputs

- Pasted text
- `.txt` files
- `.pdf` files (text extracted before parsing)

## Typical workflow

1. Open **Tools → Create from text**.
2. Paste text or load a rider file.
3. Click **Apply filter** to normalize and clean candidate lines.
4. Review and optionally edit the filtered text.
5. Click **Create** to generate scene elements.

## What gets created

Depending on parsed content, Perastage can create:

- Fixtures
- Trusses
- Hoists/motors
- Scene objects (for example, screen-like entries)

## Parsing highlights

Current parser behavior includes:

- Quantity + type fixture lines (for example, `12 Spot`)
- Compound lines split by `+` (for example, `8 Wash + 4 Beam`)
- Hang/position detection (`LX1`, `LX2`, `FLOOR`, `SIDES`, `SCREEN`, `BACKDROP`)
- Truss/pipe parsing with optional hang target
- Hoist/motor extraction from rigging lines
- Optional coordinate and margin hints in rigging targets

## Autocomplete in the editor

The dialog includes autocomplete for common workflow keywords and local dictionary terms.

Local editor shortcuts:

- `↑` / `↓` to navigate suggestions
- `Enter` or `Tab` to insert the selected suggestion
- `Esc` to close suggestions only

## After creation

- Validate results in Fixtures, Trusses, Hoists, and Objects tables.
- Cross-check placement in both 2D and 3D views.
- Save the project, then export MVR when exchange is needed.

For full parser detail and normalization rules, see [Text-to-scene rules](../developer/text_to_scene_rules.md).
