# Runtime model detail and Preferences ownership

The v1.7.0 G0.2/G0.3 block owns runtime detail policy in
`core/model_detail_policy.h`. It has no wxWidgets or GDTF dependency and uses
existing `GetValue`/`SetValue` preference services and `user_config.json` storage.
There is no schema migration or GDTF LOD interpretation.

| Config key | Canonical values | Missing/unknown default |
| --- | --- | --- |
| `viewer3d_model_detail` | `low`, `standard`, `high` | `standard` |
| `viewer3d_moving_proxy` | `0`, `1` | `0` (only `1` enables it) |

These user settings participate in MainWindow preference snapshots/reset and
Core project-load preference preservation. MVR open/merge restores a raw optional
preference snapshot around its project reset, including missing values. Preferences Apply/OK and
`EVT_UI_PREFERENCES_APPLIED` retain their existing save/refresh behavior.

## Deterministic budgets

Triangle counts and all ratios/error limits live in the Core policy:

| Representation | Untouched up to | Target ratios by source triangle count | Target limits | Relative error |
| --- | --- | --- | --- | --- |
| High | All meshes | 100% | None | No simplification |
| Standard | 2,000 | 65% through 10,000; 50% through 50,000; 40% above | 1,000–100,000 triangles | 0.002 |
| Low | 500 | 60% through 2,000; 35% through 10,000; 20% through 50,000; 10% through 200,000; 5% above | 300–25,000 triangles | 0.01 |
| Optional moving proxy | 500 | Same target ratios and limits as Low, independent of selected detail | 300–25,000 triangles | 0.01 initially |

Budgets floor the ratio result and clamp to the limits and source count.
Persistent Standard/Low use meshoptimizer's topology-aware simplification.
Budgets are best-effort: constrained geometry may retain more triangles rather
than exceed the error limit or drop isolated triangles. Low requests a lower
budget and permits a higher error than Standard. Only the temporary proxy keeps
the former sloppy-error escalation (0.01/0.02/0.05/0.10/0.20) and deterministic
triangle-subsampling fallback. Cache/overdraw optimization remains available for
all representations; the overdraw threshold is 1.05.

## Representation and cache lifecycle

`viewer3d/resources/runtime_mesh_simplification.cpp` is the extracted existing
moving-proxy meshoptimizer implementation, parameterized by the Core policy.
It copies a source mesh, discards copied GPU handles (including textures) and
flat/mirrored triangle caches, and simplifies only the copy. High resolves to
the source mesh directly. No generated detail representation is written to a
GDTF or to the existing full-geometry disk cache.

`RuntimeMeshCache` belongs to one `ResourceSyncState` and therefore one viewer
and GL context. Its key contains source mesh identity, selected detail policy,
and whether it is a moving proxy. It builds/uploads once on a cache miss.
Changing detail clears generated detail and proxy buffers; toggling only the
proxy clears proxy entries and preserves static detail entries. Unrelated
preferences do not clear this cache. Resolution observes the current preferences
on redraw, so Apply requires no project reload or base-asset reload.

`Viewer3DController::ReleaseMeshBuffers` invalidates representations for the
source before resource replacement/removal. Existing physical revision and mode
resource keys remain authoritative upstream; callback invalidation prevents
address reuse from retaining stale geometry. Controller teardown clears its
runtime cache and GPU buffers. The cache is intentionally in memory only.

Fixture instancing continues independently of simplification. All persistent
3D mesh draw paths resolve the same policy, including selection styling. The
existing navigation batching path alone requests temporary fixture proxies.
2D contexts and capture paths bypass detail resolution; picking and bounds keep
full geometry. No Layout/PDF policy changes are introduced.

## Preferences restructuring

`PreferencesDialog` is a container/coordinator with a native `wxTreebook`:

- General: Language, Units, Updates
- Import: Rider Import
- Viewer: 3D Viewer, Selection & Movement
- Formats: GDTF, MVR Import / Export

Category nodes have no page; wxWidgets displays their first child. Pages are
created and loaded once. Navigation never reloads, applies, or saves settings,
so pending edits survive switching. Each cohesive `*_preferences_page.*` under
`gui/preferences/` owns its controls and load/apply wiring. A small
`PreferencesPage` contract supplies vertical scrolling and responsive wrapping
of explanatory text and long native control labels. Native choices can shrink
with the right pane. No custom rendering/theme or settings backend is added.

The dialog applies every page through the existing preference service, commits
once, and posts the existing unit/preference events only after successful save.
The original dirty-state capture/restore remains in
`preferencesdialog_persistence.cpp`, using the existing legacy ConfigManager
bridge. Units delegates conversion of pending Rider fields to the Rider page;
Core still owns unit parsing/formatting. Language selection is in its page, while
the restart notice remains in the coordinator and retains its deduplication.
The GDTF page composes the existing mutation-policy and credential panels.

Existing persisted keys/values remain compatible; there is no migration:

| Page | Preserved keys |
| --- | --- |
| Rider Import | `rider_autopatch`, `rider_layer_mode`, `rider_lx1_height`/`pos`/`margin` through `rider_lx6_height`/`pos`/`margin` |
| Units | `ui_distance_unit_system`, `ui_weight_unit_system` |
| Language | `ui_language` |
| Updates | `app_update_startup_mode` (including the existing `never_auto` read alias) |
| GDTF | `gdtf_mutation_policy`; existing credential-vault storage and legacy `gdtf_username`/`gdtf_password` synchronization |
| MVR | `mvr_truss_geometry_export_mode` |
| Selection & Movement | `selection_group_move_fixture`, `selection_group_move_truss`, `selection_group_move_support`, `selection_group_move_scene_object`, `viewport_magnet_show_anchor_references` |
| 3D Viewer | `viewer3d_invert_orbit_horizontal`, `viewer3d_invert_orbit`, `viewer3d_render_style` |

The two new detail/proxy keys above live in the 3D Viewer page alongside navigation,
render style, and the unchanged shortcut information. The focused rendering
panel remains composed inside that page. Selection/MVR helpers accept the existing
preference facade as well as ConfigManager, preserving their domain policy and
callers. Render-style parsing remains reusable without ConfigManager; its legacy
adapter stays in Viewer3D. GPU release/detail resolution live in the adjacent
controller implementation; the unreachable duplicate fixture batch was removed.

Intentionally retained coupling: the GDTF credentials panel still uses the
existing global GUI services and platform credential vault, and its explicit
**Validate credentials** action still validates/saves credentials immediately.
That existing explicit action is independent of unapplied settings edits and is
not rolled back by Cancel. Normal page edits persist only through Apply/OK.
Credential failures keep their existing warnings/return handling; restructuring
does not redesign credential transactions or failed-save rollback. Dirty-state
bookkeeping stays behind the existing legacy bridge. Future cleanup can replace
that bridge and inject credential services without being required for v1.7.0.

Focused tests cover policy parsing/budgets, immutable source data, uploaded-copy
state, cache reuse/separation/invalidation, real preference store file/buffer
round trips, and wxWidgets panel load/apply for every detail/proxy/render-style
combination. The real-dialog regression additionally covers all eight pages,
existing values, Apply/OK/Cancel, navigation without mutation, pending edits,
unit conversion/events, language notices, and resizing. It isolates OS credential
and locale discovery with test doubles; those backends retain their separate
integration coverage. `check_preferences_page_boundary.sh` guards page ownership
and the single persistence boundary. `check_model_detail_boundary.sh` protects ownership and config-key
bookkeeping. Native visual review of demanding/non-manifold assets remains
useful because meshoptimizer budgets are intentionally approximate.

New GUI strings are gettext-ready and extracted into the POT. Language catalog
updates, including any future French/German additions, follow the final UI string
freeze; this block does not introduce languages or edit existing PO catalogs.
