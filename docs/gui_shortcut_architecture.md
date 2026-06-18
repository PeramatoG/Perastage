# GUI shortcut architecture

## Goal

Perastage uses a centralized GUI shortcut registry to keep keyboard behavior explicit,
validated and consistent across modules.

## Central registry

The canonical shortcut map lives in:

- `gui/shortcut_registry.h`
- `gui/shortcut_registry.cpp`

Each shortcut entry defines:

- **Key combination** (`keyCode` for non-modifier shortcuts).
- **Action** (`ShortcutAction`).
- **Owner module** (`ownerModule`).
- **Scope** (`ShortcutScope`: `Global`, `Viewer2D`, `Viewer3D`, `Cli`).
- **Focus policy** (`ShortcutFocusPolicy`: allowed or blocked in editable widgets).
- **Priority** (used when multiple entries share the same key across scopes).

## Current shortcut map

Menu accelerators are owned by their menu entries and are not routed through `ShortcutAction`. The Edit menu includes `Ctrl+G` for Group and `Ctrl+U` for Ungroup; both commands are global menu accelerators, operate on the active cross-table selection, and keep the existing editable-widget behavior provided by wxWidgets menu accelerator handling.


| Key | Action | Owner | Scope | Focus policy |
|---|---|---|---|---|
| `Z` | `FitView` | `viewer2d` | `Viewer2D` | Block in editable widgets |
| `Z` | `FitView` | `viewer3d` | `Viewer3D` | Block in editable widgets |
| `P` | `CliPrefillPos` | `cli` | `Cli` | Block in editable widgets |
| `R` | `CliPrefillRot` | `cli` | `Cli` | Block in editable widgets |
| `F` | `CliPrefillFixture` | `cli` | `Cli` | Block in editable widgets |
| `1` | `SelectFixturesTab` | `gui` | `Global` | Block in editable widgets |
| `2` | `SelectTrussesTab` | `gui` | `Global` | Block in editable widgets |
| `3` | `SelectSupportsTab` | `gui` | `Global` | Block in editable widgets |
| `4` | `SelectObjectsTab` | `gui` | `Global` | Block in editable widgets |
| `NumPad1` | `ViewportFront` | `viewer2d` | `Viewer2D` | Block in editable widgets |
| `NumPad3` | `ViewportSide` | `viewer2d` | `Viewer2D` | Block in editable widgets |
| `NumPad7` | `ViewportTop` | `viewer2d` | `Viewer2D` | Block in editable widgets |
| `NumPad1` | `ViewportFront` | `viewer3d` | `Viewer3D` | Block in editable widgets |
| `NumPad3` | `ViewportSide` | `viewer3d` | `Viewer3D` | Block in editable widgets |
| `NumPad7` | `ViewportTop` | `viewer3d` | `Viewer3D` | Block in editable widgets |
| `NumPad5` | `ViewportReset3D` | `viewer3d` | `Viewer3D` | Block in editable widgets |

## Priority and resolution rules

Resolution is performed by `ResolveShortcut(...)`.

1. Normalize key code to uppercase.
2. Filter shortcuts with matching key.
3. Apply focus gating through `CanExecuteShortcut(...)`:
   - reject when modifiers are active,
   - reject when policy blocks editable-focus and focus is in editable text,
   - require scope/focus match (for example, `Viewer2D` requires focus in 2D viewer).
4. Select the highest-priority shortcut among eligible entries.
5. Return one execution decision for `MainWindow::OnGlobalCharHook(...)`.

This makes shortcut behavior deterministic and keeps one single decision point for
"can this shortcut run under current focus?".

## Startup/test validation

`ValidateShortcutRegistry()` detects collisions by `(scope, normalized key)`.

- Called at `MainWindow` startup (debug assertion + error log).
- Covered by `tests/shortcut_registry_test.cpp`.

## Execution flow in GUI

1. `MainWindow::OnGlobalCharHook(...)` builds a `ShortcutExecutionContext` from focus state.
   - Editable focus is detected via `gui::IsEditableWidgetFocused(...)` in
     `gui/editable_focus_utils.{h,cpp}`.
   - The helper covers `wxTextEntry`-based controls (for example `wxTextCtrl`
     and custom text-entry widgets), editable `wxComboBox`, spin controls, and
     active `wxGrid` cell editors.
2. The hook asks `ResolveShortcut(...)` for one decision.
3. `MainWindow::ApplyShortcutDecision(...)` executes the routed action:
   - `ApplyFitShortcut()` for focused viewer fit (selection-aware: when there is
     an active selection, fit targets selected fixtures/trusses/scene objects;
     otherwise it falls back to fitting the full scene),
   - `FocusConsoleForQuickCommand(...)` for CLI-prefill actions,
   - notebook-tab selection commands for `1..4`,
   - `ApplyViewportShortcut(...)` for top/front/side numpad view actions in the active viewer,
   - 3D viewport reset for `NumPad5`.
4. Viewport orientation parity rule:
   - `ViewportFront` in `Viewer2D` and `Viewer3D` must represent the same front-facing
     convention (camera from negative Y looking toward the origin) so shortcut behavior
     is consistent across both viewers.

Viewer-specific direct handling for these migrated shortcuts is intentionally avoided,
so routing stays centralized in GUI.

## Continuous placement cancellation

`Escape` remains a viewer-local interaction key rather than a registered global
shortcut. While Add Fixture continuous placement owns the focused 2D or 3D
viewer, it takes precedence over measure-tool cancellation and discards only
the fixture currently attached to the pointer. Fixtures already confirmed with
left-click remain in the scene. Right-click provides the same cancellation
behavior without changing shortcut routing.

A left-button press is confirmed as placement only when it is released without
navigation movement. Holding and dragging keeps the normal viewer-local
navigation behavior: pan in the 2D viewer, and orbit or `Shift`-pan in the 3D
viewer. This mouse gesture does not add a shortcut-registry entry.

## Consistent editable-focus guard across local key handlers

Panels that handle local key events also reuse `gui::IsEditableWidgetFocused(...)`
before executing panel-level actions, avoiding divergent focus checks:

- `Viewer2DPanel::OnKeyDown(...)`
- `Viewer3DPanel::OnKeyDown(...)`
- `LayoutViewerPanel::OnKeyDown(...)`

This keeps key behavior consistent while editing text or cell editors, and
prevents global-style actions from stealing keys during edit sessions.

## Mouse modifier gestures tied to selection behavior

The following mouse gestures are implemented directly in viewer panels (not in
the keyboard shortcut registry), and must stay aligned between viewers when they
represent equivalent actions:

- `Ctrl + Left Drag`:
  rectangle selection in the active table context.
- `Ctrl + Shift + Left Drag`:
  transversal rectangle selection across all selectable object tables in both
  `Viewer2D` and `Viewer3D`.

## Local shortcuts in "Create scene from text" autocomplete

`RiderTextDialog` adds local key handling in the multiline text box for
autocomplete navigation, and keeps `Esc` scoped to autocomplete behavior:

- `↑` / `↓`: move the active suggestion.
- `Enter` or `Tab`: accept the selected suggestion and replace the active token.
- `Esc`: if suggestions are open, close them without inserting; if suggestions
  are closed, keep focus in the dialog editor (do not close the dialog).

The dialog also shows this shortcut help inline under the editor so users can
discover the local autocomplete controls without leaving the workflow.

Priority and focus impact:

- `Esc` is always consumed by the dialog to avoid accidental window close while
  editing rider text.
- Navigation/accept keys are consumed only when the dropdown is open.
- Outside that state, the text box keeps its standard free-text editing behavior.
- Global shortcut routing is unchanged because the dialog key handling stays local.
- Suggestion ordering uses local relevance ranking (exact/prefix/fuzzy + recent
  usage + syntax context), but key routing/priority rules above remain unchanged.
