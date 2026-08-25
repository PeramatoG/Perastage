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
   `N MOTOR <capacity>Kg FOR <hang>`.
4. Groups fixture output by detected hang (`LX1`, `LX2`, `FLOOR`, ...).
5. Normalizes physical floor aliases such as `suelo`, `piso`, `floor`,
   `deck`, `calle(s) a suelo`, and `ground lane(s)` to `FLOOR`.
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
14. The complete filter pass is idempotent: reapplying **Apply filter** to any
    canonical result produces byte-identical text.
15. Truss targets with accessory suffixes are normalized to the effective hang
    target (for example, `... + HUESITOS PARA PUENTES LX` is interpreted as
    `PUENTES LX`).
16. Hoist/motor target keywords in the filtered preview are normalized to
    English `FOR`, while `PARA` remains accepted in input text and existing
    normalized text.

After applying the filter, users can manually adjust the filtered text and then
press **Create**; the same normalization rules are still applied at creation
time.

### Preview serialization

Filtered previews use UTF-8 text with LF line endings and no carriage returns,
trailing spaces, or leading/trailing blank lines. Exactly one empty line
separates each non-empty fixture or `RIGGING` block, and empty blocks are
omitted. Reapplying the filter to its preview produces byte-identical output.

## Autocomplete keywords and ranking in "Create from text"

The multiline editor in **Tools → Create from text** exposes autocomplete to
speed up rider authoring/editing before import.

Supported built-in keywords currently include:

- Position/section terms: `RIGGING`, `LX1`, `LX2`, `LX3`, `LX4`, `SIDES`,
  `FLOOR`, `SCREEN`, `BACKDROP`.
- Rigging/object terms: `TRUSS`, `PIPE`, `MOTOR`, `FOR`, `KG`, `M`,
  `LED SCREEN`, `PRIMITIVE:CUBE`, `PRIMITIVE:CYLINDER`.
- Utility terms used by the dialog flow: `APPLY FILTER`, `HAZER`, `FAN`.

Autocomplete also includes dynamic dictionary terms loaded from:

- Fixture dictionary type names (GDTF dictionary entries).
- Truss dictionary model names.

Ranking order is relevance-based (not alphabetical-only):

1. Exact token match.
2. Prefix match.
3. Substring/fuzzy match.
4. Recent accepted suggestions get a recency boost.
5. Context boost:
   - after `FOR`, position labels (`LX*`, `SIDES`, `FLOOR`, `SCREEN`) are
     prioritized.
   - after a numeric token (for example `8`), type/model terms are prioritized.

Ranking is configurable through autocomplete ranking weights in code
(`RiderTextAutocompleteProvider::RankingWeights`), allowing score tuning
without changing matching logic.

## Input normalization

1. File type:
   - `.txt` is read as plain text.
   - `.pdf` is converted to text before parsing.
2. Parsing is line-based.
3. Windows carriage returns (`\r`) are removed from each line.
4. Inline annotations wrapped as `*(...)*` are ignored during parsing.
5. Most keyword matches are case-insensitive.

## Section detection rules

The importer classifies complete normalized heading lines rather than searching
for section words inside arbitrary descriptions. Quantity-prefixed equipment
therefore remains equipment even when its model or purpose contains words such
as `iluminar`, `control`, `video`, or `audio`.

- `ILUMINACION` / `LIGHTING` and `APARATOS` / `FIXTURES` enter fixture mode.
- `RIGGING` and `RIGGING Y ESTRUCTURAS` enter rigging mode.
- `EFECTOS` / `EFFECTS` is an ignored equipment-category boundary, not a
  physical position. An effect explicitly listed under `FLOOR` or another
  physical hang remains importable.
- `CONTROL DE ILUMINACION` / `LIGHTING CONTROL` stops fixture collection.
- `VIDEO` establishes video context without itself selecting a physical hang;
  a following screen heading selects `SCREEN` and enables screen collection.
- Audio and other recognized unrelated headings stop active collection.
- If a hang-position line appears before explicit sections, the importer assumes fixtures mode.
- If fixture list lines appear before **any recognized section header** and no
  hang header has been defined yet, the importer assumes fixtures mode and
  defaults active hang to `FLOOR`.

## Hang/position detection

Recognized hang labels:

- `LX<number>` (for example `LX1`, `LX2`, ...)
- Front/LX1: `frontal`, `frente`, `puente frontal`, `front`, `front light`,
  `front truss`, `downstage`
- Middle/LX2: `cenital`, `central`, `medio`, `puente central`, `mid`,
  `middle`, `center`, `centre`, `midstage`
- Rear/LX3: `contra`, `contraluz`, `puente trasero`, `trasero`, `back`,
  `rear`, `backlight`, `upstage`
- Sides: `calle(s)`, `lateral(es)`, `side(s)`, `side light(s)`, and descriptive
  `CALLES DIRECTAS ...` headings
- Floor: `suelo`, `piso`, `calle(s) a suelo`, `floor`, `ground`, `deck`,
  `ground lane(s)`
- Screen: `screen`, `pantalla`, `pantalla led`, `proyeccion`, `proyección`,
  `led screen`, `led wall`, `projection`
- `backdrop` / `backdrops` / `telon` / `telones` / `puente de telon(es)`

Behavior:

- Hang labels are normalized to uppercase (`LX1`, `FLOOR`, ...).
- Fixture hang headers can include extra descriptive suffix text before `:`
  (for example `CALLES EN LAYHER:`), while still keeping the detected hang
  alias (`CALLES` -> `LX SIDES`).
- Front, middle, and rear semantic headings normalize to `LX1`, `LX2`, and
  `LX3`; an explicit `LX<number>` always takes precedence.
- Physical floor aliases are normalized to `FLOOR`; `EFFECTS` is not a hang.
- Side-position aliases are normalized to the canonical value `LX SIDES`.
- Explicit normalized headers such as `LX SIDES` are accepted as hang labels
  in both filtered text and direct import input.
- Screen aliases are normalized to `SCREEN`. This allows the common
  `VIDEO` → `PANTALLA LED` / `LED SCREEN` / `LED WALL` structure to reuse the
  existing screen-object importer.
- Backdrop aliases (`backdrop(s)`, `telon(es)`, `puente de telon(es)`) are
  normalized to `BACKDROP`.
- The active hang affects fixture/truss layer naming and default placement (`Y/Z`).

## Fixture parsing rules

Accepted fixture patterns:

- `N description`
- Optional list bullets (`-` or `*`) before quantity are accepted.
- Quantity-only lines (`N`) are accepted and applied to the next non-empty line.
- `+` splits compound lines into independent fixture groups.
- A plus segment without its own quantity inherits the first segment's
  quantity (`1 FOLLOWSPOT + OPERATOR` becomes two quantity-one entries).
- Ordinary parenthesized fixture annotations are removed from the canonical
  token; no richer hidden fixture description is retained.

Examples:

- `12 Spot`
- `8 Wash + 4 Beam`
- `6` (next line: `PAR`) ⇒ `6 PAR`

For each fixture created:

1. `instanceName` is initialized with incremental numbering per parsed token (`Type 1`, `Type 2`, ...).
2. `typeName` is set from parsed text and may be refined through the GDTF dictionary.
3. If dictionary entry exists:
   - Fixture dictionary lookup is case-insensitive and ignores whitespace in
     type names (for example `MACAURA`, `Mac Aura`, and `mac aura` resolve to
     the same entry).
   - `gdtfSpec` and `gdtfMode` are assigned.
   - If dictionary color is defined, it is copied into fixture `color`.
   - If dictionary category is defined, it is copied into fixture `category`
     with source `Manual`.
   - Fixture display/type name can be replaced by the parsed GDTF fixture name when available.
   - Physical properties (weight/power) are filled from GDTF when missing.
4. `positionName` is set from current hang.
   - If no hang header was parsed and parsing started from a headerless
     fixture list (before any recognized section header), `positionName`
     defaults to `FLOOR`.
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
- Dimension parsing uses the physical meter pair and does not replace it with
  a later pixel-resolution pair such as `1664x832 PIXELS`.
- Parsed screen dimensions are applied as:
  - X (width) = parsed width
  - Z (height) = parsed height
  - Y (thickness) = fixed `0.1 m`
- Screen geometry is emitted as a `primitive:cube` entry in
  `SceneObject.geometries` using local geometry scale (same primitive pipeline
  used by regular cube scene objects), while object transform keeps position.
  The canonical cube mesh measures `1.0 m` on each axis, so the local X/Y/Z
  scale values are the final width/thickness/height in meters. Rider screen
  dimensions are therefore stored only on the geometry transform; the scene
  object basis remains unscaled.
- If no valid size is found, importer falls back to `8.0 x 5.0 m`.
- Screen objects are centered on the associated screen truss span and placed so
  their top edge sits `0.2 m` below the truss.

## Fixture color precedence

Fixture color resolution follows this order:

1. Explicit fixture color (already stored on the fixture row/object).
2. Dictionary color for the fixture `typeName`.
3. Deterministic fallback color (GDTF/model-derived fallback used by the
   current creation flow).

When changing a fixture type color from Summary, Perastage updates matching
fixtures in the scene and also persists that color in dictionary entries for
the edited type (and entries sharing the same GDTF file identity).

## Truss parsing rules

Supported truss syntax includes:

- `N truss MODEL LENGTH m [para HANG]`
- `N truss MODEL LENGTH m [HANG]` (hang can appear with or without `para`)
- `N truss MODEL LENGTH m [for HANG]`
- `N truss MODEL [para HANG]` (lengthless form, used for backdrop lines)
- `N truss MODEL [for HANG]` (lengthless form, used for backdrop lines)
- More generic fallback lines containing `truss` and a measurable length.
- Pipe aliases are also accepted: `pipe`, `pipes`, `vara`, `varas`.
  - `N pipe MODEL LENGTH m [para/for HANG]`
  - `N pipe MODEL [para/for HANG]` (defaults to `14 m`).
  - Pipe lines use the same parenthesis coordinate overrides and bracket margin
    overrides as truss lines.
- Optional coordinate override appended to hang in parentheses, e.g.
  - `LX1 (0, -1, 9)` => `x, y, z`
  - `LX1 ( -1, 9 )` => `y, z` (keeps default `x`)
  - `LX1 (7)` => `y` only (keeps default `x, z`)
  - Non-numeric text inside parentheses is ignored while extracting numbers.
  - This can be written both in truss targets (`... PARA LX1 (7)`) and in
    hang headers (`LX1 (7)` / `LX1 (7):`), where truss placement inherits the
    hang override.
  - If both are present, truss-line overrides take precedence over hang-header
    overrides for any axis values provided in the truss line.
  - The **Apply filter** pass preserves these coordinate tokens so pressing
    **Create** after filtering keeps the same truss placement overrides.
- Optional margin override appended to hang in square brackets, e.g.
  - `LX1 [0.8]` => per-rider margin override for `LX1`.
  - Can be written in truss targets (`... PARA LX1 [0.8]`) and hang headers
    (`LX1 [0.8]` / `LX1 [0.8]:`).
  - Values use the active UI distance unit system (`metric` or `imperial`).
  - If both are present, truss-line bracket overrides take precedence over
    hang-header bracket overrides for the same hang.
  - The **Apply filter** pass preserves bracket tokens so pressing **Create**
    after filtering keeps the same margin overrides.

Key truss behaviors:

1. Length is parsed in meters and converted to millimeters.
   - For `BACKDROP` lines without explicit length, importer reuses the latest
     parsed `LX*` or `SCREEN` truss span.
   - If no prior `LX*`/`SCREEN` span exists, `BACKDROP` uses `12 m` by default.
2. If model token contains dimensions like `40x40`, width/height defaults are inferred from it.
3. `para <hang>` / `for <hang>` overrides current hang.
4. Prefix cleanup supports `PUENTE`/`PUENTES` in hang names.
5. Long trusses are split into symmetric pieces using preferred segments (`3000, 2000, 1000, 500 mm`) plus a center piece when beneficial.
6. Each piece becomes a truss object with computed `x` placement and hang-based
   `y/z`, unless a coordinate override is provided in the hang token.
   Coordinate values use the active UI distance unit system (`metric` or
   `imperial`) at import time.
7. Dictionary/model resolution is attempted using normalized lookup keys.
   - Lookup also tries model-token variants that remove common finish/color
     adjectives (e.g. `NEGRO`, `BLACK`) so names like `40X40 PRO NEGRO` can
     resolve against canonical dictionary entries such as `TRUSS 40X40 PRO 3M`.
8. If model/symbol cannot be fully resolved to renderable geometry (`.3ds`/`.glb` available), importer keeps dummy-box truss data and logs a warning.

Pipe-specific behavior:

1. Pipe lines create `SceneObject` entries (object table), not truss entries.
2. Imported pipes are created as simple cylindrical placeholders:
   - length = parsed length (or `14 m` for lengthless pipe lines),
   - radius = `2.5 cm` (`50 mm` diameter).
   - Import stores a cylinder primitive geometry reference on the object so the
     3D view renders the pipe directly as a cylinder (not only as a generic
     meshless fallback box).
   - Transform path matches regular primitive cylinders for geometry sizing:
     pipe proportions are stored in `SceneObject.geometries[].localTransform`.
     The default pipe orientation (`+90°` on `Y`) is stored in
     `SceneObject.transform` together with world position.
   - Pipe object names use hang naming, not length naming: `PIPE <hang>`
     (for example `PIPE LX1`).
3. Pipe lines still create/target the same normalized hang names (`LX1`, `LX2`, ...),
   including `PUENTES LX` expansion to `LX1..LXN`.
4. Pipes do not carry truss hang-weight fields; fixtures still keep their hang
   position assignments exactly as usual.

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

- `N MOTOR <capacity><unit> [ ... ] FOR <hang>`
- `N HOIST <capacity><unit> [ ... ] FOR <hang>`
- Optional list bullets (`-` or `*`) before quantity are accepted.

Capacity parsing:

1. Recognizes case-insensitive mass units and compact forms (`500Kg`, `2TO`, `1 t`).
2. Accepted kilogram aliases: `kg`, `kgs`, `kilo`, `kilogramos`.
3. Accepted ton aliases: `t`, `to`, `tn`, `ton`, `tons`, `toneladas`.
4. Ton values are converted to kilograms (`2TO` => `2000 kg`).

Hang normalization for hoists:

- `PA`, `P.A`, `P.A.` => `PA` group behavior (position name is stored as `P.A.`).
- `SIDE FILL` and `SIDEFILL` (case-insensitive, with normalized whitespace) => `SIDEFILL`.
- `SIDE`/`SIDES`, `CALLE`/`CALLES`, and `LX SIDE`/`LX SIDES` => `LX SIDES`; floor phrases such as `CALLES A SUELO` remain `FLOOR`.
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
4. **`LX SIDES` hoists (`SIDES`, `CALLES`)**
   - Quantity is split between left/right side trusses (`left = ceil(N/2)`,
     `right = floor(N/2)`).
   - Each side set is distributed along the side truss length on `Y`, preserving
     the truss direction (`startY -> endY`) and using a 1 m margin from both ends.
   - Hoists use each side truss own `X` anchor and `Z` height.
   - If side trusses are missing, importer falls back to side fixture anchors.
5. **SCREEN hoists**
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
  - `LX SIDES` targets => `SIDE L <index>`, `SIDE R <index>` (ordered by `Y`
    inside each side).
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
  - Pipe objects: `pos <hang>`
- `type` mode:
  - Fixtures: `fix <fixture type>`
  - Trusses: `truss <hang>`
  - Pipe objects: `obj <hang>`
- Hoists (both modes): `rig <hoist function>`

Missing/empty names fall back to the default scene layer.

## Fixture distribution over trusses/pipes

After parsing, imported fixtures are distributed per hang:

1. Truss span for each hang is inferred from imported truss pieces.
2. Margin comes from `rider_lx*_margin` (for LX hangs), unless a square-bracket
   override (`[value]`) was provided for that hang in the imported rider.
3. If no truss is found for a hang, fallback spacing is centered around origin with 500 mm steps.
   - Exception for `LX SIDES`: fixtures are still split into left/right groups
     as if side trusses existed, with fallback anchors at `Y = 1.0 m`
     and `500 mm` spacing moving upstage (positive Y).
4. Ordering mode per hang:
   - Default mode is **symmetric by type**:
     - odd counts place a center fixture,
     - remaining fixtures are paired left/right.
   - If a fixture type is listed more than once for the same hang in the rider
     fixture list (for example `MegaPointe ... Spiider ... MegaPointe`), that
     hang switches to **linear left-to-right mode** and keeps rider list order.
5. Fixtures are split into placement groups:
   - **Bottom group**: all categories except `Wash`, `Blinder`, `Strobe`.
   - **Bottom-back override**: `Wash` is placed on the back side.
   - **Top-front group**: `Blinder` and `Strobe` are placed on top-front.
   - **Smoke side-floor group**: `Smoke` fixtures are placed using side-style
     mirroring (left/right split along `Y`) at floor level.
6. `x` spacing is computed independently per group and per ordering mode:
   - Bottom fixtures share one spacing/order pass.
   - Top-front fixtures use a separate spacing/order pass and do not affect bottom spacing.
   - This means bottom and top-front groups can each resolve symmetric vs
     linear behavior independently while keeping their own list order.
7. If a group has exactly one fixture, it is placed at the horizontal center of
   the usable hang span (instead of at a lateral edge).
8. Final fixture transform by group:
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
9. `LX SIDES` fixture distribution:
   - Fixtures are split into two symmetric groups (left/right side).
   - Each side group is distributed along `Y` (matching side truss direction).
   - `X` anchors match side-truss anchors (or fallback anchors when no side truss exists).
10. `Smoke` fixture distribution (all non-`LX SIDES` hangs):
   - `Smoke` fixtures are split into two symmetric groups (left/right side),
     distributed along `Y` using side-truss extents when available.
   - Without side trusses, each side uses fallback `Y = 1.0 m` with `500 mm`
     spacing moving upstage (same side-style fallback spacing).
   - `Z` is forced to floor (`0 mm`).
   - `X` anchors:
     - with real side trusses: same side-truss anchors as `LX SIDES`,
     - fallback without side trusses: `0.5 m` inside the widest detected `LX*`
       truss span (`left = minX + 0.5 m`, `right = maxX - 0.5 m`).
10. If a hang is created from `pipe`/`vara` syntax, all fixture categories on
    that hang use the same direct placement (`x` distribution + shared `baseY/baseZ`);
    no extra front/back/top offsets are applied.

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

## Auto-color behavior after text scene creation

After a successful **Tools → Create from text** import:

- The same **Auto color** routine used after MVR import is executed.
- Fixture/layer colors are normalized immediately, including fallback random
  colors for fixture groups with no dictionary color. Fixtures without a GDTF
  library match are grouped by their normalized parsed type name, so repeated
  instances of the same missing fixture type receive the same color.
- This runs before the final scene refresh, so users see the colored result as
  soon as text import completes.

## Config keys that affect import

- `rider_layer_mode` (`position` or `type`)
- `rider_autopatch` (`1`/enabled by default, `0` to disable)
- `rider_lx1_height` .. `rider_lx6_height`
- `rider_lx1_pos` .. `rider_lx6_pos`
- `rider_lx1_margin` .. `rider_lx6_margin`

## Console transform command rule contract

The console transform parser also defines user-visible behavior that must stay
stable across releases:

1. `rot x|y|z <values>` keeps per-element rotation behavior (existing contract).
2. `rot x|y|z <values> --group` and `rot x|y|z <values> --g` rotate the full
   current selection as one rigid group.
3. Group rotation default pivot is the center of the selected elements'
   axis-aligned bounding box (bbox center), computed from selected fixtures,
   trusses, supports, and scene objects.
4. If the command ends with `x,y,z`, that triplet is used as pivot override.
   Coordinates are interpreted in meters (same user-facing unit convention as
   other console position inputs) and converted to internal millimeters.
5. For group rotation, element orientation and position are both updated so the
   selection behaves as a single transformed block around the chosen pivot.

## Maintenance protocol (mandatory when rules change)

When changing any text-to-scene parsing or placement behavior:

1. Update this file (`docs/developer/text_to_scene_rules.md`) in the same PR.
2. If user-visible behavior changes, update Help/README references.
3. Add or update tests under `tests/` covering the new/changed rule.
4. Mention the exact rule change in the PR description.

## Suggested user-doc excerpt

Perastage parses rider text by identifying sections, hang positions, fixture lines, and truss declarations, then generates fixtures/trusses with predictable layer, placement, numbering, and auto-patch rules. PDF riders are first converted to text and parsed using the same rule set.

### Truss dictionary and resource resolution

- Dictionary matching is case- and whitespace-insensitive. Known finish words
  such as `NEGRO`, `BLACK`, and `PLATA` are removed only as complete tokens from
  both model candidates and length-bearing truss names; explicit lengths remain
  part of the candidate key.
- A GDTF path identifies the source archive. An extracted `.glb` or `.3ds` is
  the renderable 3D symbol resource. An optional GDTF SVG remains a 2D resource
  and is not assigned to the truss 3D `symbolFile`.
- Valid metadata-only or SVG-only archives may provide truss metadata, but
  Rider import retains deterministic dummy geometry when no supported 3D model
  exists. Imports never fabricate a model path or borrow another hang's
  resource for an unresolved truss.
- Rider truss resource tests distinguish supported 3D resource-path resolution
  from mesh rendering validation. GLB/3DS decoding and native mesh dimensions
  remain owned by the dedicated model-loader tests.
- Truss dictionary imports are copied into the active dictionary's managed
  storage. Dictionary-backed Rider trusses use that managed GDTF archive for
  `modelFile` and `gdtfSpec`; a transient import source is not their stable
  resource identity.
- A `PERASTAGE_LIBRARY_PATH` override root must already exist and be writable.
  Isolated tests create that root before resolving the active dictionary so
  transient sources and normal user-data fallback storage never become test
  fixture ownership locations.
