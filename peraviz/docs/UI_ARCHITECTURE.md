# UI Architecture

## Purpose

This document defines how Peraviz UI responsibilities are split so that new features can grow without turning the viewer into a monolithic script.

## Module map

The UI is organized into five module families.

### 1. Shell

**Responsibility**
- Own app-level startup and top-level composition.
- Instantiate and wire major UI modules.
- Keep global state transitions explicit and minimal.

**Typical contents**
- Main scene root and top-level startup scripts.
- High-level routing between windows/panels.

### 2. Panels

**Responsibility**
- Own focused, embedded UI blocks that live inside a parent surface.
- Render controls and status views for one bounded concern.

**Typical contents**
- Fixture test panel.
- DMX monitor panel.
- Small reusable controls grouped by workflow.

### 3. Windows

**Responsibility**
- Own detachable or standalone dialogs/windows.
- Group settings that are not required in the always-visible flow.

**Typical contents**
- Visual Settings window.
- Future advanced diagnostics windows.

### 4. Controllers

**Responsibility**
- Translate UI intent into runtime actions.
- Orchestrate calls to scene loading, selection, DMX runtime, and rendering services.

**Typical contents**
- Scene load controller.
- Fixture interaction controller.
- Settings apply/reset controller.

### 5. Policies

**Responsibility**
- Hold decision logic that must stay stable and testable.
- Define defaults, gating rules, and visibility levels.

**Typical contents**
- Feature-level visibility policy (User/Advanced/Debug).
- "Clean screen by default" gating logic.
- Safe defaults and opt-in behavior policies.

## Visibility tiers: User / Advanced / Debug

Every new UI surface must be assigned to one of these tiers.

### User
- Visible by default.
- Required for core viewer workflow.
- Must minimize cognitive load and visual noise.

### Advanced
- Hidden behind explicit opt-in (toggle, menu action, or advanced section).
- Useful for power users but not required to load and inspect scenes.
- Must preserve simple defaults when disabled.

### Debug
- Intended for development/validation only.
- Must never block normal end-user workflow.
- Should be isolated from User-tier paths and controlled through policy gates.

## Naming conventions

### Files and scripts
- Use clear, responsibility-first names.
- Prefer suffixes by role:
  - `*_panel.gd` for embedded panels.
  - `*_window.gd` for standalone windows.
  - `*_controller.gd` for orchestration logic.
  - `*_policy.gd` for visibility/default rules.

### Node names
- Use stable, explicit names matching responsibility.
- Avoid ambiguous generic names such as `Manager`, `Helper`, or `UIStuff`.

### Public methods
- Start with intent verbs (`show_*`, `hide_*`, `apply_*`, `reset_*`, `update_*`).
- Keep ownership obvious: a panel updates panel widgets; a controller applies runtime effects.

## Script ownership rules

- A script owns exactly one primary responsibility.
- Panel scripts should not directly implement cross-module business logic.
- Controllers coordinate modules; they should not render widget-level details.
- Policies provide decisions; they should be free of scene-tree side effects whenever possible.
- Shared helpers must live close to the module that owns the behavior, not in a global dump file.

## UX requirement: clean screen by default

"Clean screen by default" is a mandatory UX requirement.

### Requirement definition
- On first launch (or after reset), the default UI must prioritize scene readability over controls density.
- Only User-tier essentials remain visible by default.
- Advanced and Debug surfaces must be opt-in.

### Acceptance criteria
- A new user can load and inspect an MVR scene without dismissing intrusive overlays.
- Debug instrumentation is not shown unless explicitly enabled.
- Enabling advanced/debug views does not permanently pollute default startup state.
