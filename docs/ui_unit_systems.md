# UI Unit Systems (Metric/Imperial)

This document describes how Perastage handles user-facing unit systems after the metric/imperial UI support changes.

## Canonical internal units

Perastage keeps a single canonical representation internally:

- **Distance:** millimeters (**mm**)
- **Weight:** kilograms (**kg**)

The selected UI system (Metric or Imperial) only affects how values are **displayed**, **parsed from text input**, and **labeled** in tables/dialogs.

### Why this matters

- Data consistency across modules (tables, labels, import dialogs, 2D/3D overlays).
- No schema split or duplicated scene state for metric vs imperial.
- Stable project persistence and deterministic conversion paths.

## Config keys and scope

The active UI systems are stored in user config:

- `ui_distance_unit_system`: `metric` or `imperial`
- `ui_weight_unit_system`: `metric` or `imperial`

These keys drive formatting/parsing in dialogs and tables (fixtures/trusses/hoists/objects, rider import fields, support labels, etc.).

## Display and conversion rules

### Distance

Canonical distance is mm.

- Metric display = `mm / 1000` -> meters (`m`)
- Imperial display = `mm / 304.8` -> feet (`ft`)

### Weight

Canonical weight is kg.

- Metric display = `kg` (`kg`)
- Imperial display = `kg * 2.2046226218487757` -> pounds (`lb`)

## Parsing rules by input text

Input parsing is case-insensitive and trims whitespace.

### Distance parsing to mm

Order of parsing:

1. **Feet/inches notation** (`5' 6"`, `5'6"`, `5'`)  
   -> interpreted as feet + inches, converted using `25.4 mm/in`.
2. **Explicit suffix** (`m`, `ft`, `in`)  
   Examples: `2.5m`, `12ft`, `18in`.
3. **Plain number without suffix**  
   Interpreted in the currently selected UI distance system:
   - Metric UI: number means **meters**
   - Imperial UI: number means **feet**

### Weight parsing to kg

Order of parsing:

1. **Explicit suffix** (`kg`, `lb`)  
   Examples: `10kg`, `220lb`.
2. **Plain number without suffix**  
   Interpreted in the currently selected UI weight system:
   - Metric UI: number means **kilograms**
   - Imperial UI: number means **pounds**

### Practical implications

- Explicit suffixes always override the selected UI system.
- A plain numeric value follows the active UI system.
- Invalid text is rejected by parser call sites that require numeric conversion.

## Rounding/precision policy (formatting)

Perastage uses fixed decimal formatting by UI context:

### Distance

- **Table:** 3 decimals
- **Label (compact/UI labels):** 2 decimals
- **Inspector/editor precision:** 4 decimals

### Weight

- **Table:** 2 decimals
- **Label (compact/UI labels):** 1 decimal
- **Inspector/editor precision:** 3 decimals

This is a **display/formatting policy**. Canonical internal values remain in mm/kg and are not re-quantized globally.

## Examples (editing and visualization)

## Metric UI examples

Distance system = Metric, Weight system = Metric.

- Editing `height` with `6.2` -> interpreted as `6.2 m` -> stored internally as `6200 mm`.
- Editing `height` with `18ft` -> explicit suffix accepted -> stored as `5486.4 mm` even in Metric UI.
- Editing `weight` with `120` -> interpreted as `120 kg`.
- Editing `weight` with `220lb` -> explicit suffix accepted -> stored as `~99.79 kg`.
- Visualization labels show `m` and `kg` suffixes.

## Imperial UI examples

Distance system = Imperial, Weight system = Imperial.

- Editing `height` with `20` -> interpreted as `20 ft` -> stored as `6096 mm`.
- Editing `height` with `5' 6"` -> parsed as feet/inches -> stored as `1676.4 mm`.
- Editing `height` with `2.5m` -> explicit suffix accepted -> stored as `2500 mm`.
- Editing `weight` with `220` -> interpreted as `220 lb` -> stored as `~99.79 kg`.
- Editing `weight` with `100kg` -> explicit suffix accepted -> stored as `100 kg`.
- Visualization labels show `ft` and `lb` suffixes.

## Notes for maintainers

- Keep all new persistence/data-model fields in canonical mm/kg when representing distances/weights.
- Any new UI editor/renderer should reuse the units helper API (`core/units/units.*`) for formatting/parsing consistency.
- If a new UI context is introduced, define and document its decimal precision explicitly.
