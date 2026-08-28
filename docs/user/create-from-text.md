# Create from Text

Before creating the scene, Perastage checks every unique fixture type against
the active fixture dictionary. Unknown types can be reviewed against cached
GDTF Share suggestions without signing in merely to open the review. Downloads
begin only after confirmation, accepted mappings are remembered for later
riders, and Generic remains available for a one-time fallback. When a selected
profile has several DMX modes, you must choose the intended mode explicitly.

The **Resolve fixture types** window appears whenever at least one referenced
fixture has no valid file in the active dictionary, including when no catalog is
available. **Use suggested** accepts a strong exact catalog match, **Search...**
opens the existing GDTF Share browser for the selected alias, and **Use generic**
continues with the one-import fallback without saving a dummy mapping. Selected
profiles are downloaded only after **Resolve and create** is pressed. Cancelling
closes the workflow before any scene content or dictionary mapping is changed.

Successful GDTF resolutions are applied when possible. A recoverable download,
validation, mode, authentication, or dictionary error changes only the affected
fixture type to Generic fallback; scene creation continues with the remaining
safe import plan. Downloaded GDTF Share sources retain their external identity
and are not mislabeled as Perastage-generated derivatives.

Generic fallback is the visible default and never blocks **Resolve and create**.
Safe suggestions are optional; suggestions without a chosen mode fall back to
Generic. Screen objects and video-control equipment are not treated as lighting
fixtures. Cached catalog matching runs after the resolver is visible, and an
empty or unavailable catalog leaves the offline Generic workflow usable.
On a new installation without a usable cache, Perastage makes one catalog
acquisition attempt through the same GDTF Share sign-in and validated refresh
workflow as **Tools > Download GDTF**. Cancelling sign-in or losing network
access still leaves Generic fallback available.
Catalog sign-in, download, parsing, and matching progress remain in the
**Resolve fixture types** progress area; only the credential-entry dialog opens
separately when user input is required.
Automatic catalog matching skips rows already resolved by the active dictionary
or an explicit selection, so the matching total represents unresolved rows only.

Fixture types can be corrected directly in the table without changing the
source rider text. The **Create** checkbox can omit every fixture represented
by a row while preserving trusses, screens, hoists, and other rider objects.
Manual GDTF search uses the combined manufacturer and fixture identity.
Downloaded profiles use a portable catalog-based filename, while their GDTF
Share revision ID remains the internal download and deduplication identity.

The resolver shows a compact progress gauge while loading and parsing the
catalog, followed by real per-fixture matching progress. Its final summary stays
visible, and the Status column uses distinct colors for dictionary, modified,
automatic, user-assigned, Generic, and skipped outcomes. **Resolve and create**
remains available while matching runs.

Double-click a row's **Mode** cell to choose from its valid modes. Dictionary
rows show the mapped local GDTF filename, and confirmed dictionary-mode changes
are saved only after **Resolve and create**.

Importing a rider from a `.txt` or `.pdf` file uses the same preflight and
fallback behavior as pasting rider text.

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
- `EFECTOS` / `EFFECTS` equipment imported on `FLOOR`, with atmospheric
  fixtures retaining the existing mirrored, floor-level Smoke placement
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
