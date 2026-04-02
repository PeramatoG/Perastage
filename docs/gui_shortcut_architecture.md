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
   - `ApplyFitShortcut()` for focused viewer fit,
   - `FocusConsoleForQuickCommand(...)` for CLI-prefill actions,
   - notebook-tab selection commands for `1..4`,
   - 3D viewport standard/reset view commands for numpad actions.

Viewer-specific direct handling for these migrated shortcuts is intentionally avoided,
so routing stays centralized in GUI.

## Consistent editable-focus guard across local key handlers

Panels that handle local key events also reuse `gui::IsEditableWidgetFocused(...)`
before executing panel-level actions, avoiding divergent focus checks:

- `Viewer2DPanel::OnKeyDown(...)`
- `Viewer3DPanel::OnKeyDown(...)`
- `LayoutViewerPanel::OnKeyDown(...)`

This keeps key behavior consistent while editing text or cell editors, and
prevents global-style actions from stealing keys during edit sessions.
