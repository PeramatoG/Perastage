# Text-to-scene rules (Rider importer)

This document explains the rules currently applied by Perastage when creating a scene from rider text loaded from `.txt` or extracted from `.pdf`.

It is intended to be the **single source of truth** for:

- User-facing documentation (Help/manual).
- Internal maintenance when parser rules change.
- QA validation of expected text-to-scene behavior.

## Scope and entry points

The feature is exposed in:

- **Tools → Create from text** (dialog import from pasted text or rider file).
- Rider file import (`.txt` and `.pdf`).

Implementation entry points:

- `RiderImporter::Import(path)`
- `RiderImporter::ImportText(text)`
- `RiderImporter::LoadText(path)`
- `RiderImporter::BuildFixtureFilterPreview(text)` (UI filter pass before create)

## Text filter step in "Create from text"

The dialog includes an **Apply filter** button that runs the same line
selection rules used by fixture parsing, and replaces the editor content with
the filtered result before scene creation.

Filter behavior:

1. Keeps only lines interpreted as fixture entries in fixture sections.
2. Keeps truss/rigging lines and emits a normalized `RIGGING` block.
3. Keeps motor/hoist lines found in rigging and normalizes them as
   `N MOTOR <capacity>Kg PARA <hang>`.
4. Groups fixture output by detected hang (`LX1`, `LX2`, `FLOOR`, ...).
5. Normalizes floor aliases to `FLOOR`:
   - `floor`
   - `efecto` / `efectos`
   - `calle a suelo` / `calles a suelo`
   - `ground lane` / `ground lanes`
6. Truss lines using `PUENTES LX` are expanded as `LX1`, `LX2`, `LX3`, ...
7. Hoist lines using `PUENTES LX` are expanded and distributed over detected
   `LX*` targets in the same filtered rigging block.
8. Truss hang alias `PANTALLA` is normalized to `SCREEN`.
9. Truss lines normalized to `FLOOR` are skipped in filtered output/import.
10. Expands `+` compound lines into individual fixture lines.
11. Supports quantity-only lines (`N` followed by description on next line).
12. Removes parenthesized notes from fixture tokens to reduce rider noise.
13. The filter pass is idempotent for normalized truss lines ending in
    `SCREEN` (re-applying **Apply filter** keeps those lines).

After applying the filter, users can manually adjust the filtered text and then
press **Create**.

## Input normalization

1. File type:
   - `.txt` is read as plain text.
   - `.pdf` is converted to text before parsing.
2. Parsing is line-based.
3. Windows carriage returns (`\r`) are removed from each line.
4. Most keyword matches are case-insensitive.

## Section detection rules

The importer keeps parsing state with sections (fixtures/rigging/control):

- Enters **fixtures mode** when a line contains: `ilumin`, `robotica`, or `convencion`.
- Enters **rigging mode** when a line contains: `rigging`.
- Leaves active import sections (fixtures/rigging) when a line contains terms like:
  `sonido`, `audio`, `control de p.a.`, `monitores`, `microfon`, `video`,
  `realizacion`, or `control`.
- If a hang-position line appears before explicit sections, the importer assumes fixtures mode.

## Hang/position detection

Recognized hang labels:

- `LX<number>` (for example `LX1`, `LX2`, ...)
- `screen` / `pantalla` / `led screen`
- `floor`
- `efecto` / `efectos`
- `calle a suelo` / `calles a suelo`
- `ground lane` / `ground lanes`

Behavior:

- Hang labels are normalized to uppercase (`LX1`, `FLOOR`, ...).
- Floor aliases (`floor`, `efecto(s)`, `calle(s) a suelo`, `ground lane(s)`)
  are normalized to `FLOOR`.
- Screen aliases (`screen`, `pantalla`, `led screen`) are normalized to
  `SCREEN`.
- The active hang affects fixture/truss layer naming and default placement (`Y/Z`).

## Fixture parsing rules

Accepted fixture patterns:

- `N description`
- Optional list bullets (`-` or `*`) before quantity are accepted.
- Quantity-only lines (`N`) are accepted and applied to the next non-empty line.
- `+` splits compound lines into independent fixture groups.

Examples:

- `12 Spot`
- `8 Wash + 4 Beam`
- `6` (next line: `PAR`) ⇒ `6 PAR`

For each fixture created:

1. `instanceName` is initialized with incremental numbering per parsed token (`Type 1`, `Type 2`, ...).
2. `typeName` is set from parsed text and may be refined through the GDTF dictionary.
3. If dictionary entry exists:
   - `gdtfSpec` and `gdtfMode` are assigned.
   - Fixture display/type name can be replaced by the parsed GDTF fixture name when available.
   - Physical properties (weight/power) are filled from GDTF when missing.
4. `positionName` is set from current hang.
5. Initial hang coordinates are injected (`Y`, `Z`) and later refined by distribution logic.

## Truss parsing rules

Supported truss syntax includes:

- `N truss MODEL LENGTH m [para HANG]`
- More generic fallback lines containing `truss` and a measurable length.

Key truss behaviors:

1. Length is parsed in meters and converted to millimeters.
2. If model token contains dimensions like `40x40`, width/height defaults are inferred from it.
3. `para <hang>` overrides current hang.
4. Prefix cleanup supports `PUENTE`/`PUENTES` in hang names.
5. Long trusses are split into symmetric pieces using preferred segments (`3000, 2000, 1000, 500 mm`) plus a center piece when beneficial.
6. Each piece becomes a truss object with computed `x` placement and hang-based `y/z`.
7. Dictionary/model resolution is attempted using normalized lookup keys.
8. If model/symbol cannot be fully resolved to renderable geometry (`.3ds`/`.glb` available), importer keeps dummy-box truss data and logs a warning.

Special case:

- If hang is exactly `LX`, quantity `N` expands to `LX1..LXN`.
- If hang is `SCREEN` and there is no dedicated screen config key, trusses are
  placed from the last created LX truss reference, with `Y = last_lx_y - 1m`
  and `Z = last_lx_z - 0.5m`. If no LX truss has been created yet, importer
  falls back to the highest configured LX (`height > 0`) using the same
  offsets.

## Hoist (motor) parsing and placement rules

Supported hoist syntax includes:

- `N MOTOR <capacity><unit> [ ... ] PARA <hang>`
- `N HOIST <capacity><unit> [ ... ] PARA <hang>`
- Optional list bullets (`-` or `*`) before quantity are accepted.

Capacity parsing:

1. Recognizes case-insensitive mass units and compact forms (`500Kg`, `2TO`, `1 t`).
2. Accepted kilogram aliases: `kg`, `kgs`, `kilo`, `kilogramos`.
3. Accepted ton aliases: `t`, `to`, `tn`, `ton`, `tons`, `toneladas`.
4. Ton values are converted to kilograms (`2TO` => `2000 kg`).

Hang normalization for hoists:

- `PA`, `P.A`, `P.A.` => `PA` group behavior (position name is stored as `P.A.`).
- `SIDE FILL` => `SIDEFILL`.
- `PANTALLA` => `SCREEN`.
- `PUENTE(S) LX` => `LX` distribution mode.

Placement defaults:

1. **PA/P.A. hoists**
   - Split into left/right groups around `LX1`.
   - Anchored 1 m outside `LX1` truss span (`X`), same `Y/Z` as `LX1`.
   - Group internals use 1 m spacing; if needed, additional items form a grid.
2. **SIDEFILL hoists**
   - Split left/right around `LX1` truss span with 1 m outside offset in `X`.
   - Placed 2 m behind `LX1` on `Y`.
3. **LX hoists (`PUENTES LX`)**
   - Quantity is distributed across detected `LX*` trusses (`LX1`, `LX2`, ...).
   - Each per-LX set is placed on that truss at the same `Y/Z`, using a 2 m
     margin from truss ends in `X`.
4. **SCREEN hoists**
   - Distributed equidistantly along the `SCREEN` truss span.

Created hoists:

- Are stored as `Support` objects.
- Use capacity in kilograms.
- Are assigned one of the default dummy hoist profile ids by capacity range:
  `dummy_standard_500kg`, `dummy_standard_1000kg`, or `dummy_standard_2000kg`.
- Hoist function defaults by target:
  - `PA`, `P.A.`, `SIDEFILL`, `OUTFILL` => `Audio`
  - `SCREEN`, `LEDSCREEN`, `VIDEO` => `Video`
  - Any other target => `Lighting`

## Layer assignment rules

Driven by config key `rider_layer_mode`:

- `position` mode:
  - Fixtures: `pos <hang>`
  - Trusses: `pos <hang>`
- `type` mode:
  - Fixtures: `fix <fixture type>`
  - Trusses: `truss <hang>`

Missing/empty names fall back to the default scene layer.

## Fixture distribution over trusses

After parsing, imported fixtures are distributed per hang:

1. Truss span for each hang is inferred from imported truss pieces.
2. Margin comes from `rider_lx*_margin` (for LX hangs).
3. If no truss is found for a hang, fallback spacing is centered around origin with 500 mm steps.
4. Ordering alternates fixture types symmetrically:
   - odd counts place a center fixture,
   - remaining fixtures are paired left/right.
5. Final fixture transform:
   - `x`: evenly spaced from start to end,
   - `y`: front side of truss (`baseY - trussWidth/2`),
   - `z`: hang height.

## Numbering and identity rules

Only **newly imported fixtures** are renumbered/reassigned:

- Existing fixtures keep their `fixtureId`, `unitNumber`, name, and transform.
- Imported fixtures are grouped by fixture type and assigned ID blocks of 100:
  - Type A: `101..`
  - Next type: `201..`
  - etc., continuing in 100-size blocks.
- Starting block is computed from the highest existing fixture ID.
- Unit numbers continue from the highest existing unit number detected for the same type.

## Auto-patch behavior

At the end of import:

- Auto-patch runs by default.
- It is skipped only when config `rider_autopatch = 0`.

## Config keys that affect import

- `rider_layer_mode` (`position` or `type`)
- `rider_autopatch` (`1`/enabled by default, `0` to disable)
- `rider_lx1_height` .. `rider_lx6_height`
- `rider_lx1_pos` .. `rider_lx6_pos`
- `rider_lx1_margin` .. `rider_lx6_margin`

## Maintenance protocol (mandatory when rules change)

When changing any text-to-scene parsing or placement behavior:

1. Update this file (`docs/text_to_scene_rules.md`) in the same PR.
2. If user-visible behavior changes, update Help/README references.
3. Add or update tests under `tests/` covering the new/changed rule.
4. Mention the exact rule change in the PR description.

## Suggested user-doc excerpt

Perastage parses rider text by identifying sections, hang positions, fixture lines, and truss declarations, then generates fixtures/trusses with predictable layer, placement, numbering, and auto-patch rules. PDF riders are first converted to text and parsed using the same rule set.

## Peraviz MVR/GDTF visual fallback rule

When loading MVR/GDTF content in Peraviz, dummy placeholder meshes must only be rendered if no real visual geometry exists for that node subtree. If a valid model (mesh/scene) is present in descendants, placeholder cubes/cones are removed to avoid double drawing.
