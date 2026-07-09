# GDTF mutation policy (Perastage)

This document defines how Perastage writes and version-stamps GDTF files, with a focus on `description.xml` mutations and long-term compatibility behavior.


## Export canonicalization policy

Perastage is permissive when importing GDTF/MVR data and strict when exporting it. Loading a legacy or externally generated GDTF must not rewrite the user's source file just because it was read, but every GDTF that leaves Perastage is normalized through the shared `core/gdtf_canonicalizer` layer before it is written directly or embedded in an MVR archive.

The canonicalizer enforces the official GDTF `FixtureType` child order (`AttributeDefinitions`, `Wheels`, `PhysicalDescriptions`, `Models`, `Geometries`, `DMXModes`, `Revisions`, `FTPresets`, `Protocols`), removes non-standard `FixtureType` children such as legacy `PerastageMutationAudit`, preserves valid standard sections and resources, validates required root/fixture structure, and repairs Perastage-owned placeholder `FixtureTypeID` values with deterministic stable IDs when safe.

Any canonicalization mutation is recorded with a standard GDTF `Revision` only. Perastage-specific GDTF metadata must not use custom XML nodes; legacy `PerastageMutationAudit` is read-only compatibility metadata and is never written on export. Perastage-specific MVR metadata remains restricted to root-level `GeneralSceneDescription/UserData/Data[@provider="Perastage"]`; object-level MVR `UserData` is not exported for Fixture, SceneObject, Support, Truss, or GroupObject.

## 1) Inventory of GDTF write points in Perastage

Perastage currently mutates GDTF archives at the following integration points.

| Module | File | Function(s) | Mutation scope |
|---|---|---|---|
| Viewer 3D API | `viewer3d/gdtfloader.cpp` | `SetGdtfProperties(...)` | Writes `PhysicalDescriptions/Properties` (`Weight`, `PowerConsumption`), appends a standard `Revision`, rewrites `.gdtf`. |
| GUI symbol workflow | `gui/windows/symbol_fixture_applier.cpp` | `RewriteGdtf(...)` + `AppendMutationAuditMetadata(...)` (called by `ApplySymbolsToFixtureGdtf(...)`) | Writes/updates SVG symbol assets and model SVG offsets, appends a standard `Revision`, rewrites `.gdtf`. |
| Project symbol cache manifest | `core/symbol_cache_manifest.cpp` | `SymbolCacheManifest::ValidateFixture(...)`, `MarkFixtureSymbolsValid(...)` | Stores project-level metadata in `.pstg` packages so startup can skip deep GDTF inspection only when the referenced GDTF hash and required view metadata still match. |
| Truss GDTF generation | `core/truss_gdtf_builder.cpp` | `BuildTrussGdtfFromInstance(...)`, `ConvertLegacyGtrussToGdtf(...)`; `gui/trusseditdialog.cpp` calls the builder when truss type fields are edited | Creates Perastage-owned truss GDTF archives with a standard `Structure` root geometry, deterministic `FixtureTypeID`, standard `Revision`, and no custom XML nodes. |
| MVR export patching | `mvr/mvrexporter.cpp` | `CreatePatchedGdtf(...)` + export resource canonicalization | Creates temporary patched GDTF copies for export overrides (manufacturer/model/physical properties/color/dimensions), appends a standard `Revision` when patched, and canonicalizes every `.gdtf` before it is packaged. |
| Shared audit helpers | `core/gdtf_mutation_audit.cpp` | `StampPerastageMutationMetadata(...)`, `AppendRevision(...)`, `ApplyPhysicalPropertiesWithAudit(...)` | Centralizes standard GDTF revision-appending semantics used by write flows. |

### Notes on ownership

- `core/gdtf_mutation_audit.{h,cpp}` is the single owner of Perastage GDTF revision semantics.
- `core/gdtf_canonicalizer.{h,cpp}` is the shared owner of export-time GDTF structural canonicalization and validation.
- Write call sites in other modules must use this helper API instead of hand-rolling custom revision XML shapes.
- Truss GDTF files generated, completed, normalized, or modified by Perastage are exported as derived Perastage-owned copies named `Manufacturer@Model@Perastage.gdtf`; external or library source GDTF files are read as inputs and are not overwritten. When the Trusses table edit dialog changes a GDTF-specific type field for a model-only truss, Perastage creates that derived GDTF immediately and attaches it to the project while leaving MVR-only instance edits project-scoped.
- GDTF model dimensions are written in meters. Perastage truss dimensions are stored in millimeters, so truss GDTF generation converts length, width, and height from millimeters to meters at export time.
- Fixture SVG symbols remain stored inside their corresponding GDTF files; the `.pstg` symbol cache manifest stores metadata only and never stores SVG payloads.
- Fixture display color is no longer persisted by mutating `description.xml` model `Color` values in place. The persisted source of truth for default color selection is the Perastage dictionary/project data layer, while `GetGdtfModelColor(...)` remains read-only for legacy fallback reads.

## 2) Exact `Revision` format used by Perastage

Perastage appends `<Revision />` under `<FixtureType>/<Revisions>` and ensures `<Revisions>` exists when needed. Repeated exports do not append a duplicate Perastage revision when the same `Text`, `ModifiedBy`, and `UserID` already exist.

Perastage writes these attributes:

- `Date`: UTC timestamp in ISO-8601 `YYYY-MM-DDTHH:MM:SSZ`.
- `ModifiedBy`: caller-provided string, or default value returned by `BuildPerastageModifiedBy()` (`"Perastage " + perastage::build_info::appVersion()`).
- `Text`: human-readable action summary (for example: model color update, fixture symbol views applied, MVR export patch action).
  - Note: this remains an example because MVR export patching can still record color-related patch actions in the exported temporary copy, but the direct in-place model-color mutation API was removed.
- `UserID`: integer; default `0`.

Canonical shape:

```xml
<Revisions>
  <Revision
    Date="2026-04-05T10:20:30Z"
    ModifiedBy="Perastage 0.x.y"
    Text="Applied fixture SVG symbol views (top, side, front, bottom)"
    UserID="0"/>
</Revisions>
```

## 3) Legacy `PerastageMutationAudit` compatibility

- Symbol: `GdtfMutationAudit::kPerastageGdtfMutationSchemaVersion`.
- Location: `core/gdtf_mutation_audit.h`.
- Current value: `1`.
- Meaning: legacy version of the former **Perastage-owned mutation metadata contract**, not the GDTF specification version.

Perastage no longer writes `<PerastageMutationAudit .../>` into GDTF `description.xml`, because it is not a standard GDTF node. The importer and compatibility helpers may still read and ignore legacy nodes from older files.

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
   - Fixture symbol validation requires the Perastage top, bottom, front, and side SVG views before project manifest cache entries can skip deep inspection.
2. **Revision correctness**
   - No new `<PerastageMutationAudit>` node is written.
   - A new `<Revision>` entry is appended with valid `Date`, `ModifiedBy`, `Text`, `UserID` when Perastage intentionally changes a GDTF.
   - No duplicate identical Perastage revision is appended on repeated exports.
3. **Compatibility safety**
   - Legacy/no-audit and unknown-schema files remain readable through fallback behavior.
4. **Archive integrity**
   - Rewritten `.gdtf` keeps `description.xml` readable and required referenced resources present.
   - Exported `.gdtf` content has official `FixtureType` child order and contains no `PerastageMutationAudit` node.
5. **No workflow regressions**
   - Existing fixture rendering/import/export/symbol flows continue operating for non-mutated files.

## 6) Verification checklist

### Manual checklist

- [ ] Apply fixture symbol generation to a fixture and verify the `.gdtf` receives updated top, bottom, front, and side SVG entries and offsets.
- [ ] Edit fixture color via dictionary/project workflow and verify fixture color persists after reopening without mutating model `Color` in the source `.gdtf`.
- [ ] Edit fixture physical properties and verify weight/power values persist after reopening.
- [ ] Inspect resulting `description.xml` and verify no new `PerastageMutationAudit` node is present, `FixtureType` children follow the official order, and `Revision` attributes follow the policy format.
- [ ] Export an MVR that triggers patched GDTF overrides and verify the exported patched GDTF carries standard revision metadata.
- [ ] Confirm legacy GDTF (without `PerastageMutationAudit`) remains loadable.
- [ ] Confirm unknown/future `SchemaVersion` triggers safe fallback behavior (no hard failure due only to version mismatch).

### Automated tests relevant to this policy

- `tests/gdtf_canonicalizer_test.cpp`
  - validates export canonicalization order, legacy metadata removal, stable placeholder FixtureTypeID repair, and duplicate revision prevention.
- `tests/gdtfloader_set_properties_test.cpp`
  - validates physical-properties mutation and persisted XML changes.
- `tests/symbol_fixture_applier_gdtf_test.cpp`
  - validates symbol write path + mutation audit compatibility behavior.
- `tests/symbol_cache_manifest_test.cpp`
  - validates project-level symbol cache manifest fallback and update safety behavior.
- `tests/check_perastage_tree_modules.sh`
  - mandatory architecture/module boundary consistency check.
- `tests/check_no_configmanager_get_in_gui.sh`
  - mandatory guardrail for GUI access patterns.

## Project Fixture Apply adapter policy (Checkpoint 08B)

Project Fixture Weight and PowerConsumption edits are now owned by a non-GUI apply adapter. The adapter consumes a session-built apply request, validates the stable fixture UUID, resolved operational GDTF path, selected mode, finite non-negative physical values, and explicit `GdtfWritePolicy` before mutating files or project data.

`ReadOnly` rejects document mutation. `OverwriteOwnedFile` is reserved for verified Perastage-owned files. `CreateDerivativeBeforeMutation` creates or updates the stable Perastage derivative through the existing dictionary behavior before applying physical values, keeping the original source unchanged. Unsupported policies fail without project attachment. Any derivative created before a later write failure is reported as a partial external side effect and is not attached to the project.

After a successful physical write, the adapter propagates Weight and PowerConsumption to fixtures in the resulting source/type family using fixture UUIDs, marks the values as GDTF-sourced, clears physical dirty state, and reports only positions whose effective Weight changed. Type, source reference, and mode context selections are applied to the target fixture; selected mode is not propagated to unrelated fixtures.
