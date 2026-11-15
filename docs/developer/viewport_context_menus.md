# Viewport context menus

## Verified causes and historical behavior

Commit `c20005054a36` (7 February 2026) introduced selection shortcuts on empty
space. Its element-hit early returns were intentional: it did not implement an
individual fixture editor menu. `a0e65b3eed42` (20 July) extended that design to
trusses. Consequently the old 2D handler explicitly skipped a fixture/truss hit.
This was missing functionality, rather than a menu ownership regression.

`74e7aff35362` (1 August) extracted the 3D selection-submenu policy and guarded
unavailable/empty selection menus. Its `selectionObjectHit` suppressed only the
selection shortcuts. Render style and Export image remained available on hits
and when picking was unavailable. Both right-up event bindings existed; stack
menu lifetime and transferred submenu ownership were valid.

A separate verified suppression path existed before menu construction:
`OnCaptureLost` called `NavigationSession::End()`, which deliberately retained
`HasMoved()`. `OnRightUp` returned on that flag without consuming/resetting it.
There was no right-down binding to start a fresh click. Losing capture after
camera movement could therefore suppress every subsequent right-click until
another gesture cleared the flag. The navigation policy extraction
(`b6dd4d61`) retained this path; this analysis does not attribute the original
introduction of the defect to that refactor.

Capture loss now resets navigation completely. Each viewer has a separate
right-button gesture tracker: press resets it, motion beyond three logical
pixels suppresses the popup even if the pointer returns, and release consumes
it. Active navigation/selection gestures block it. Escape and capture loss
cancel it. Placement and line-point right-click cancellation take precedence.
No right-button camera navigation binding has been added.

## Ownership and selection

`viewer_common/context_menu_model.*` replaces the old 3D-only action policy. It
has no wxWidgets, OpenGL, ConfigManager or renderer dependency. Typed UUID hits,
effective selection, normal click/group membership, interaction state and
capabilities produce ordered actions and a selection replacement decision.
The original clicked UUID remains the individual editing target.

Each viewport keeps its normal picking adapter. The 2D handler uses
`ResolvePickForScope` and `Viewer2DInteractionScopePolicy`; 3D uses the same
`QueryHoverUuid`/`QueryHoverUuidAcrossTables` paths as ordinary selection. Content
scale conversion, visibility and depth confirmation remain owned by those
picking services. There is no stale hover fallback. Cross-table actions retain
the current preference and picking priority. Merely requesting a menu does not
reload GDTFs or rebuild geometry.

`gui/viewport_context_menu.*` constructs and dispatches the wx menu. Runtime IDs
are held by `wxWindowIDRef` until dispatch completes, avoiding fixed-range
collisions and leaked reservations. Submenus transfer ownership to their parent.
On an already-selected effective member, no selection setters run: typed
membership, direct groups, chronology and highlights stay intact. An unselected
hit uses the existing group-expansion policy, with the configured movement
preferences, and publishes through `RefreshAfterToolSelectionUpdate` and
`SetSceneSelection`. Selection-only menu preparation creates no scene Undo entry.
Empty space preserves selection and exposes the existing type/position shortcuts.

Intentional viewport differences remain: only 3D offers Render style and Export
image; 2D camera-only Layout editors keep scene interaction disabled. The 3D
renderer currently has fixture/truss/object picking targets, without a dedicated
hoist pick target. Hoists are directly contextual in 2D and participate in mixed
selections/groups and supported collective actions in both viewers. This change
does not introduce another 3D picking backend.

## Reused editing and command contracts

- Fixture and truss editors reuse the existing table activation handlers and
  dialogs. UUIDs resolve through stable item identities at invocation, including
  sorted tables and inactive notebook pages. Editors retain their validation,
  resource ownership, refresh and history paths.
- Primitive editing is offered only when the existing primitive/screen/pipe
  editor can handle the object. The capability query uses that editor's actual
  classification logic. Conversion invokes the existing scene-to-truss handler
  and command with the clicked source UUID, preserving its same-model scope.
- Position Tool focuses the existing panel. Group, Ungroup, Copy and Delete
  dispatch existing application commands with their current validation/history
  behavior. Unsupported direct-group-only destructive actions are omitted.
- Both viewport double-click patch handlers now share `viewport_scene_editing`.
  Numeric validation precedes acceptance; `SetFixturePrimaryPatch` prepares a
  private candidate and preserves canonical DMX side assignments. Failure and
  no-op publish nothing. Success uses `GuiProjectMutationHost` for one snapshot,
  change notification and Undo/Redo, then the existing tool scene refresh.
  The prior handlers directly wrote `Fixture.address`/ID without history and
  bypassed canonical assignment validation.

No alignment implementation or controls are added. A future action enum can
carry Align Top/Bottom/Left/Right/Center Horizontally/Center Vertically and
forward to the existing alignment command IDs/service. Applicability must come
from that service; toolbar reference chronology, work plane and insertion-point
policy remain authoritative.

## Validation

`Viewer3DContextMenuModel` now tests the shared model: UUID targeting, preserved
multi-selection order, group membership, all categories, omitted unsupported
editors, empty-space capabilities, active interactions and drag recovery.
Existing selection, grouping, command, clipboard, alignment and navigation suites
remain relevant contract tests.

The native smoke uses production application objects and real wx popup loops:

```sh
cmake --build build/wsl-x64-debug --target Perastage viewer3d_context_menu_model_test
xvfb-run -a -s '-screen 0 1600x1000x24' \
  env LIBGL_ALWAYS_SOFTWARE=1 python3 tests/run_viewport_regression_smoke.py \
  build/wsl-x64-debug --source tests/viewport_context_menu_smoke.cpp
```

It observes menu-open events, dismisses with native Escape, exercises fixture
and patch editor cancellation, selection chronology, cross-table targeting,
hidden-fixture exclusion, right-drag suppression/recovery, actual camera movement
and capture-loss recovery. It verifies atomic validated patch Undo/Redo and
Group dispatch/Undo through the production command handler. Linux wxGTK/Xvfb checks do not establish Windows/macOS native
popup behavior; those platforms still need their native checks.
