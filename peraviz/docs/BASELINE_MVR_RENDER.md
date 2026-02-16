# Baseline MVR Render Validation

## Reference MVR scene set

Use at least the following reference scenes when validating render behavior before/after changes:

1. **Fixture-heavy stage**: one MVR with multiple fixture types and nested GDTF geometries.
2. **Rigging-focused scene**: one MVR with Truss/Support/SceneObject geometry and Symbol expansion.
3. **Mixed hierarchy scene**: one MVR with deep ChildList nesting to validate parent/child transform composition.

> Keep these files versioned in the internal QA asset pack. If a scene is replaced, update this document with the replacement and reason.

## Definition of “same as before”

A render is considered equivalent to baseline when all of these are true:

- **Composition**: visible objects are in equivalent relative positions in frame (fixtures, trusses, supports, scene objects).
- **Perceived scale**: no object category appears globally larger/smaller than baseline beyond tolerance.
- **General orientation**: up/forward direction and left-right layout remain consistent with baseline captures.
- **Placements**: fixture pivots and geometry parts remain attached to their expected parent nodes and do not drift.

## Acceptable tolerances

Allowed deviations for baseline comparisons:

- **Debug overlays / helper visuals**: minor differences are acceptable when they do not alter object transforms.
- **Floating-point jitter**: tiny transform differences caused by numeric precision are acceptable.
- **Logging format differences**: ordering/verbosity changes are acceptable if semantic values match.

Not acceptable:

- Rotations that change fixture aiming direction.
- Position shifts that alter patch intent or spacing.
- Scale drift that affects perceived model size.

## Operational checklist (before and after each transform-related change)

1. Load each reference MVR scene and capture baseline view(s).
2. Enable runtime flag `peraviz_debug_baseline` for comparative transform logs.
3. Record node counts and category totals from `[PeravizNative] load_mvr` summary logs.
4. Spot-check representative fixtures, trusses, supports, and scene objects.
5. Confirm parent/child hierarchy still composes correctly in deep nested nodes.
6. Compare camera-matched screenshots before vs after change.
7. Review `[PeravizBaseline]` logs for unexpected transform deltas.
8. If any transform correction is introduced, ensure `[PeravizTransformAdjustment]` logs include explicit reason tied to verified previous error.
9. Disable `peraviz_debug_baseline` and re-run one scene to ensure runtime behavior is unchanged without debug logging.
10. Attach validation evidence (screenshots + log excerpt) to PR.

## Runtime flag

Set `peraviz_debug_baseline=true` in `peraviz/project.godot` (or override it at runtime in Project Settings) to enable comparative transform logs.

- Enabled behavior:
  - Emits `[PeravizBaseline]` logs with source local matrix and mapped Godot transform.
  - Keeps transform math unchanged (logging only).
- Always-on behavior:
  - Emits `[PeravizTransformAdjustment]` when a deliberate transform normalization/correction path is executed, with explicit reason.

## Transform adjustment logging policy

Any transform adjustment must:

1. Have an explicit reason in logs.
2. Reference a demonstrated prior issue (spec mismatch, reproduced bug, or validated incorrect output).
3. Preserve traceability by logging values before and after adjustment.

Do not add silent transform fixes.
