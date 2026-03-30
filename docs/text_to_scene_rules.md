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

`Create` also runs that same filter pass internally. This keeps scene creation
deterministic whether users click **Create** directly after loading text or
click **Apply filter** first and then **Create**.

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
9. Truss hang aliases `BACKDROP`, `BACKDROPS`, `TELON`, `TELONES` are
   normalized to `BACKDROP`.
10. Truss lines normalized to `FLOOR` are skipped in filtered output/import.
11. Expands `+` compound lines into individual fixture lines.
12. Supports quantity-only lines (`N` followed by description on next line).
13. Removes parenthesized notes from fixture tokens to reduce rider noise.
14. The filter pass is idempotent for normalized truss lines ending in
    `SCREEN` (re-applying **Apply filter** keeps those lines).

After applying the filter, users can manually adjust the filtered text and then
press **Create**; the same normalization rules are still applied at creation
time.

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
- `backdrop` / `backdrops` / `telon` / `telones` / `puente de telon(es)`
- `floor`
- `efecto` / `efectos`
- `calle a suelo` / `calles a suelo`
- `ground lane` / `ground lanes`
- `calle` / `calles`
- `side` / `sides`

Behavior:

- Hang labels are normalized to uppercase (`LX1`, `FLOOR`, ...).
- Fixture hang headers can include extra descriptive suffix text before `:`
  (for example `CALLES EN LAYHER:`), while still keeping the detected hang
  alias (`CALLES` -> `LX SIDES`).
- Floor aliases (`floor`, `efecto(s)`, `calle(s) a suelo`, `ground lane(s)`)
  are normalized to `FLOOR`.
- Side-position aliases (`calle(s)`, `side(s)`) are normalized to `LX SIDES`.
- Explicit normalized headers such as `LX SIDES` are accepted as hang labels
  in both filtered text and direct import input.
- Screen aliases (`screen`, `pantalla`, `led screen`) are normalized to
  `SCREEN`.
- Backdrop aliases (`backdrop(s)`, `telon(es)`, `puente de telon(es)`) are
  normalized to `BACKDROP`.
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
   - If dictionary category is defined, it is copied into fixture `category`
     with source `Manual`.
   - Fixture display/type name can be replaced by the parsed GDTF fixture name when available.
   - Physical properties (weight/power) are filled from GDTF when missing.
4. `positionName` is set from current hang.
   - In layer-by-position mode, side fixtures use `pos LX SIDES` when side
     trusses exist and fallback to `pos SIDES` when no side truss is present.
   - Side fixtures are ordered as a linear sequence and then mirrored by side:
     first half rises on the left side, second half rises on the right side in
     reverse order.
5. Initial hang coordinates are injected (`Y`, `Z`) and later refined by distribution logic.
6. Importer falls back to category `Unknown` only when category is still empty
   after the above steps (for example, dummy fixtures without usable GDTF data).

Special screen-object handling:

- When current hang is `SCREEN` and a fixture line contains `screen` or
  `pantalla`, the importer creates **scene objects** instead of fixtures.
- Size parsing looks for `<width>x<height>` values in meters (accepted forms
  include `8x5`, `8x5m`, `8m x 5m`).
- Parsed screen dimensions are applied as:
  - X (width) = parsed width
  - Z (height) = parsed height
  - Y (thickness) = fixed `0.1 m`
- If no valid size is found, importer falls back to `8.0 x 5.0 m`.
- Screen objects are centered on the associated screen truss span and placed so
  their top edge sits `0.2 m` below the truss.

## Truss parsing rules

Supported truss syntax includes:

- `N truss MODEL LENGTH m [para HANG]`
- `N truss MODEL [para HANG]` (lengthless form, used for backdrop lines)
- More generic fallback lines containing `truss` and a measurable length.
- Optional coordinate override appended to hang in parentheses, e.g.
  - `LX1 (0, -1, 9)` => `x, y, z`
  - `LX1 ( -1, 9 )` => `y, z` (keeps default `x`)
  - `LX1 (7)` => `y` only (keeps default `x, z`)
  - Non-numeric text inside parentheses is ignored while extracting numbers.
  - This can be written both in truss targets (`... PARA LX1 (7)`) and in
    hang headers (`LX1 (7)` / `LX1 (7):`), where truss placement inherits the
    hang override.
  - The **Apply filter** pass preserves these coordinate tokens so pressing
    **Create** after filtering keeps the same truss placement overrides.

Key truss behaviors:

1. Length is parsed in meters and converted to millimeters.
   - For `BACKDROP` lines without explicit length, importer reuses the latest
     parsed `LX*` or `SCREEN` truss span.
   - If no prior `LX*`/`SCREEN` span exists, `BACKDROP` uses `12 m` by default.
2. If model token contains dimensions like `40x40`, width/height defaults are inferred from it.
3. `para <hang>` overrides current hang.
4. Prefix cleanup supports `PUENTE`/`PUENTES` in hang names.
5. Long trusses are split into symmetric pieces using preferred segments (`3000, 2000, 1000, 500 mm`) plus a center piece when beneficial.
6. Each piece becomes a truss object with computed `x` placement and hang-based
   `y/z`, unless a coordinate override is provided in the hang token.
   Coordinate values use the active UI distance unit system (`metric` or
   `imperial`) at import time.
7. Dictionary/model resolution is attempted using normalized lookup keys.
8. If model/symbol cannot be fully resolved to renderable geometry (`.3ds`/`.glb` available), importer keeps dummy-box truss data and logs a warning.

Special case:

- If hang is exactly `LX`, quantity `N` expands to `LX1..LXN`.
- If hang resolves to `LX SIDES`, importer creates two mirrored side trusses
  (left/right) per split piece:
  - `X` anchor: `0.5 m` outside the widest detected `LX*` truss span.
  - `Y` direction: truss axis is aligned along `Y` (not `X`).
  - `Z` (height): fixed at `5.0 m`.
  - If no `LX*` truss exists, importer uses a fallback reference span of `[-3, +3] m`.
- If no side truss exists but `LX*` trusses are available, side fixtures still
  use mirrored fallback anchors at `0.5 m` outside the widest detected `LX*`
  truss span.
- If hang is `SCREEN` and there is no dedicated screen config key, trusses are
  placed from the last created LX truss reference, with `Y = last_lx_y + 1m`
  and `Z = last_lx_z - 0.5m`. If no LX truss has been created yet, importer
  falls back to the highest configured LX (`height > 0`) using the same
  offsets.
- If hang is `BACKDROP`, trusses are placed `1 m` behind the latest parsed
  `LX*` or `SCREEN` truss (`+Y`) and keep the same height (`Z`).

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
   - Each per-LX set is placed on that truss at the same `Y/Z`, using a 1 m
     margin from truss ends in `X`.
   - Direct `LX<number>` hoist targets also use a 1 m margin from truss ends.
4. **SCREEN hoists**
   - Distributed using internal truss divisions (for `N` hoists, truss span is
     split into `N+1` equal parts and hoists are placed on internal cuts).

Created hoists:

- Are stored as `Support` objects.
- Use capacity in kilograms.
- Are assigned one of the default dummy hoist profile ids by capacity range:
  `dummy_standard_500kg`, `dummy_standard_1000kg`, or `dummy_standard_2000kg`.
- Hoist function defaults by target:
  - `PA`, `P.A.`, `SIDEFILL`, `OUTFILL` => `Audio`
  - `SCREEN`, `LEDSCREEN`, `VIDEO` => `Video`
  - Any other target => `Lighting`
- Hoist naming defaults (ordered left-to-right on `X`):
  - `LX*` targets => `<position> <index>` (`LX1 1`, `LX1 2`, ...).
  - `SCREEN`/video targets => `SCR <index>`.
  - `BACKDROP` targets => `BACKDROP <index>`.
  - `SIDEFILL` => `SF L`, `SF R` (adds index when side has more than one hoist).
  - `PA`/`P.A.` => `PA L <index>`, `PA R <index>`.
  - Fallback targets => `RP <index>` (Rigging Point).
- Hoists are assigned to rig layers by function:
   - `rig Audio`, `rig Video`, `rig Lighting`, etc.
   - Missing rig layers are created automatically.
   - Newly created rig layers use these default colors:
     `Audio=#FF0000`, `Video=#00FF00`, `Scenic=#0000FF`, `Extra=#8F00FF`,
     `Other=#C7A3C7`, `Lighting=#FF00FF`.
   - Hoist symbols are drawn with white fill and use their current layer color
     for the colored regions/outline, so changing a hoist to another layer
     updates the symbol color automatically.
- Hoist loads are auto-distributed after import for each hang position that
  received newly created hoists:
  - Position total uses the same rigging table rule: sum of fixture+truss+hoist
    weights in the hang, then `+5%`, then rounded up to the next 5 kg.
  - If hoists in that hang are not collinear, total is split equally by hoist
    count and each share is rounded up to the next 5 kg.
  - If hoists are collinear and count is `2..8`, the importer applies standard
    percentage factors (`2: 50/50`, `3: 19/62/19`, ... , `8: 6/16/14/14/14/14/16/6`),
    then rounds each hoist load up to the next 5 kg.

## Layer assignment rules

Driven by config key `rider_layer_mode`:

- `position` mode:
  - Fixtures: `pos <hang>`
  - Trusses: `pos <hang>`
- `type` mode:
  - Fixtures: `fix <fixture type>`
  - Trusses: `truss <hang>`
- Hoists (both modes): `rig <hoist function>`

Missing/empty names fall back to the default scene layer.

## Fixture distribution over trusses

After parsing, imported fixtures are distributed per hang:

1. Truss span for each hang is inferred from imported truss pieces.
2. Margin comes from `rider_lx*_margin` (for LX hangs).
3. If no truss is found for a hang, fallback spacing is centered around origin with 500 mm steps.
   - Exception for `LX SIDES`: fixtures are still split into left/right groups
     as if side trusses existed, with fallback anchors at `Y = 1.0 m`
     and `500 mm` spacing moving upstage (positive Y).
4. Ordering alternates fixture types symmetrically:
   - odd counts place a center fixture,
   - remaining fixtures are paired left/right.
5. Fixtures are split into placement groups:
   - **Bottom group**: all categories except `Wash`, `Blinder`, `Strobe`.
   - **Bottom-back override**: `Wash` is placed on the back side.
   - **Top-front group**: `Blinder` and `Strobe` are placed on top-front.
   - **Smoke side-floor group**: `Smoke` fixtures are placed using side-style
     mirroring (left/right split along `Y`) at floor level.
6. `x` spacing is computed independently per group:
   - Bottom fixtures share one spacing/order pass.
   - Top-front fixtures use a separate spacing/order pass and do not affect bottom spacing.
7. Final fixture transform by group:
   - Bottom-front:
     - `x`: evenly spaced from start to end (bottom group),
     - `y`: `baseY - trussWidth/2`,
     - `z`: `baseZ`.
   - Bottom-back (`Wash`):
     - `x`: from bottom group spacing,
     - `y`: `baseY + trussWidth/2`,
     - `z`: `baseZ`.
   - Top-front (`Blinder`, `Strobe`):
     - `x`: evenly spaced from start to end (top group),
     - `y`: `baseY - trussWidth/2`,
     - `z`: `baseZ + trussWidth/2`.
8. `LX SIDES` fixture distribution:
   - Fixtures are split into two symmetric groups (left/right side).
   - Each side group is distributed along `Y` (matching side truss direction).
   - `X` anchors match side-truss anchors (or fallback anchors when no side truss exists).
9. `Smoke` fixture distribution (all non-`LX SIDES` hangs):
   - `Smoke` fixtures are split into two symmetric groups (left/right side),
     distributed along `Y` using side-truss extents when available.
   - Without side trusses, each side uses fallback `Y = 1.0 m` with `500 mm`
     spacing moving upstage (same side-style fallback spacing).
   - `Z` is forced to floor (`0 mm`).
   - `X` anchors:
     - with real side trusses: same side-truss anchors as `LX SIDES`,
     - fallback without side trusses: `0.5 m` inside the widest detected `LX*`
       truss span (`left = minX + 0.5 m`, `right = maxX - 0.5 m`).

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
