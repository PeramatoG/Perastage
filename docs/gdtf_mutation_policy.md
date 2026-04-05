# GDTF mutation policy (Perastage)

This document defines how Perastage writes and version-stamps GDTF files, with a focus on `description.xml` mutations and long-term compatibility behavior.

## 1) Inventory of GDTF write points in Perastage

Perastage currently mutates GDTF archives at the following integration points.

| Module | File | Function(s) | Mutation scope |
|---|---|---|---|
| Viewer 3D API | `viewer3d/gdtfloader.cpp` | `SetGdtfModelColor(...)` | Writes model `Color` in `description.xml`, stamps `PerastageMutationAudit`, appends `Revision`, rewrites the `.gdtf` archive. |
| Viewer 3D API | `viewer3d/gdtfloader.cpp` | `SetGdtfProperties(...)` | Writes `PhysicalDescriptions/Properties` (`Weight`, `PowerConsumption`), stamps audit metadata, appends `Revision`, rewrites `.gdtf`. |
| GUI symbol workflow | `gui/windows/symbol_fixture_applier.cpp` | `RewriteGdtf(...)` + `AppendMutationAuditMetadata(...)` (called by `ApplySymbolsToFixtureGdtf(...)`) | Writes/updates SVG symbol assets and model SVG offsets, stamps audit metadata, appends `Revision`, rewrites `.gdtf`. |
| MVR export patching | `mvr/mvrexporter.cpp` | `CreatePatchedGdtf(...)` | Creates temporary patched GDTF copies for export overrides (manufacturer/model/physical properties/color/dimensions), stamps audit metadata and appends `Revision` when patched. |
| Shared audit helpers | `core/gdtf_mutation_audit.cpp` | `StampPerastageMutationMetadata(...)`, `AppendRevision(...)`, `ApplyPhysicalPropertiesWithAudit(...)` | Centralizes Perastage-owned mutation metadata and revision-appending semantics used by write flows. |

### Notes on ownership

- `core/gdtf_mutation_audit.{h,cpp}` is the single owner of Perastage mutation-audit schema semantics.
- Write call sites in other modules must use this helper API instead of hand-rolling custom audit XML shapes.

## 2) Exact `Revision` format used by Perastage

Perastage appends `<Revision />` under `<FixtureType>/<Revisions>` and ensures `<Revisions>` exists when needed.

Perastage writes these attributes:

- `Date`: UTC timestamp in ISO-8601 `YYYY-MM-DDTHH:MM:SSZ`.
- `ModifiedBy`: caller-provided string, or default value returned by `BuildPerastageModifiedBy()` (`"Perastage " + app::kVersion`).
- `Text`: human-readable action summary (for example: model color update, fixture symbol views applied, MVR export patch action).
- `UserID`: integer; default `0`.

Canonical shape:

```xml
<Revisions>
  <Revision
    Date="2026-04-05T10:20:30Z"
    ModifiedBy="Perastage 0.x.y"
    Text="Applied fixture SVG symbol views (top, side)"
    UserID="0"/>
</Revisions>
```

## 3) Definition of `kPerastageGdtfMutationSchemaVersion`

- Symbol: `GdtfMutationAudit::kPerastageGdtfMutationSchemaVersion`.
- Location: `core/gdtf_mutation_audit.h`.
- Current value: `1`.
- Meaning: version of the **Perastage-owned mutation metadata contract**, not the GDTF specification version.

This schema version controls semantics for the `<PerastageMutationAudit .../>` node, currently stamped with:

- `SchemaVersion`
- `PerastageVersion`
- `PerastageVersionDisplay`
- `LastMutationDateUtc`

## 4) Compatibility matrix (legacy / current / future reads)

Compatibility is resolved by `InspectCompatibility(...)` in `core/gdtf_mutation_audit.cpp`.

| Input metadata state | Decision mode | Behavior |
|---|---|---|
| No `<PerastageMutationAudit>` node (legacy file) | `LegacyFallback` | Treat as legacy-compatible; parse symbols/fixture data with fallback logic. |
| `<PerastageMutationAudit SchemaVersion="1">` | `KnownPerastageVersion` | Treat metadata as trusted/current Perastage schema. |
| `<PerastageMutationAudit>` present but missing/invalid `SchemaVersion` | `SafeFallbackUnknownVersion` | Treat as unknown schema; keep safe fallback behavior and emit warning. |
| `<PerastageMutationAudit SchemaVersion="N">` where `N != 1` | `SafeFallbackUnknownVersion` | Treat as future/unknown schema; keep safe fallback behavior and emit warning. |

Policy intent:

- Legacy reads must keep working.
- Current known schema should be fully supported.
- Future unknown schemas must degrade safely (no hard parse failure exclusively due to schema mismatch).

## 5) Functional non-regression criteria

A GDTF mutation change is accepted only if all conditions below hold:

1. **Mutation correctness**
   - Target payload mutation is present (for example color/properties/SVG view assets+offsets).
2. **Audit correctness**
   - `<PerastageMutationAudit>` exists and includes the current schema version.
   - A new `<Revision>` entry is appended with valid `Date`, `ModifiedBy`, `Text`, `UserID`.
3. **Compatibility safety**
   - Legacy/no-audit and unknown-schema files remain readable through fallback behavior.
4. **Archive integrity**
   - Rewritten `.gdtf` keeps `description.xml` readable and required referenced resources present.
5. **No workflow regressions**
   - Existing fixture rendering/import/export/symbol flows continue operating for non-mutated files.

## 6) Verification checklist

### Manual checklist

- [ ] Apply fixture symbol generation to a fixture and verify the `.gdtf` receives updated SVG entries and offsets.
- [ ] Edit fixture color via GDTF mutation path and verify model color updates persist after reopening.
- [ ] Edit fixture physical properties and verify weight/power values persist after reopening.
- [ ] Inspect resulting `description.xml` and verify `PerastageMutationAudit` + `Revision` attributes follow the policy format.
- [ ] Export an MVR that triggers patched GDTF overrides and verify the exported patched GDTF carries mutation audit and revision metadata.
- [ ] Confirm legacy GDTF (without `PerastageMutationAudit`) remains loadable.
- [ ] Confirm unknown/future `SchemaVersion` triggers safe fallback behavior (no hard failure due only to version mismatch).

### Automated tests relevant to this policy

- `tests/gdtfloader_set_model_color_audit_test.cpp`
  - validates color mutation + audit/revision stamping behavior.
- `tests/gdtfloader_set_properties_test.cpp`
  - validates physical-properties mutation and persisted XML changes.
- `tests/symbol_fixture_applier_gdtf_test.cpp`
  - validates symbol write path + mutation audit compatibility behavior.
- `tests/check_perastage_tree_modules.sh`
  - mandatory architecture/module boundary consistency check.
- `tests/check_no_configmanager_get_in_gui.sh`
  - mandatory guardrail for GUI access patterns.
