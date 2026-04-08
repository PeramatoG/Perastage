# Documentation Policy

This document defines how Perastage documentation should be organized and maintained. The primary rule is that `README.md` stays short and acts as a landing page, while detailed workflows live in focused files under `docs/`.

## README Scope

- Keep `README.md` as an elevator pitch and quick-start entry point.
- Limit it to project overview, highlights, minimal build/run steps, and links to deeper guides.
- Do not move long troubleshooting logs, packaging checklists, or platform-specific edge cases into `README.md`.

## Detailed Documentation Scope

Use dedicated documents for deeper topics:

- `docs/features.md` for feature breakdowns and workflow summaries.
- `docs/build.md` for dependency, CMake, and advanced build options.
- `docs/installation_windows.md` for Windows-specific setup notes.
- `docs/packaging.md` for installer and desktop integration behavior.
- `docs/troubleshooting.md` for platform-specific failure modes and fixes.
- Existing policy/spec files (for example `docs/text_to_scene_rules.md` and `docs/gdtf_mutation_policy.md`) for behavior contracts.

## Formatting Rules

- Use `##` headings for major sections and `###` for subtopics.
- Keep paragraphs short and focused.
- Prefer bullet lists and tables for structured information.
- Use fenced code blocks for commands and snippets.
- Write all documentation and code comments in English.

## Localization Policy for Spanish Help (ES)

When updating the Spanish block in `help.md` (`<!-- LANG:es -->`), keep UI terminology stable and searchable:

- Do **not** translate exact UI labels for menus, panels, dialogs, tabs, and buttons.
- Translate only explanatory narrative text around those labels.
- Keep command names and option labels exactly as shown in the application UI.
- Reuse the same canonical UI terms across all ES help sections.

Minimum glossary of terms that must remain untranslated:

| Keep in English | Notes |
| --- | --- |
| `Fixtures` | Tab/panel label. |
| `Trusses` | Tab/panel label. |
| `Hoists` | Tab/panel label. |
| `Objects` | Tab/panel label. |
| `Render style` | 3D Viewer context-menu entry. |
| `Create from text` | Tools workflow entry. |
| `Apply filter` | Create-from-text action button. |
| `Create` | Create-from-text action button. |
| `File`, `Edit`, `View`, `Tools` | Menu names. |
| `Console`, `Layers`, `Layouts`, `Summary`, `Rigging`, `2D Viewer`, `3D Viewer`, `2D Render Options`, `Layout View` | Panel/view names. |

## Cross-Linking Rules

- Avoid duplicating large sections across files.
- Link related guides using relative links.
- When adding a new major workflow, add one short bullet to `README.md` Highlights and place details in the appropriate `docs/*.md` file.


## Documentation Synchronization Checklist

Use this checklist in every PR that changes behavior, UX labels, or parsing contracts to keep user help and technical rules aligned.

### 1) User-facing functionality that must be reflected in `help.md`

Update `help.md` when a change is visible to operators in normal workflows, including at least:

- New or removed menu actions, dialogs, toolbar actions, or panel entries.
- Changes to user-executed workflows (new steps, reordered steps, renamed actions, changed defaults).
- New or changed keyboard/mouse shortcuts.
- New or changed console commands, command syntax, or command scope/selection behavior.
- Build-gated visibility changes that affect what users can access in Debug vs Release.
- Any user-visible warning, limitation, or prerequisite needed to avoid incorrect usage.

### 2) Detailed rules that remain in `docs/text_to_scene_rules.md`

Keep parser/scene-generation contracts in `docs/text_to_scene_rules.md` as the technical source of truth, including:

- Token grammar and normalization expectations (headers, separators, accepted aliases).
- Parsing precedence and conflict resolution rules.
- Placement/distribution algorithms, defaults, fallback behavior, and override hierarchy.
- Coordinate/unit parsing semantics and value interpretation rules.
- Error handling, skipped-line policy, and deterministic behavior requirements.

`help.md` should summarize only operator-facing behavior. Avoid duplicating the full parser contract there; link to `docs/text_to_scene_rules.md` for details.

### 3) Criteria for updating both files in the same PR

Update **both** `help.md` and `docs/text_to_scene_rules.md` in the same PR when **any** of the following is true:

- A text-to-scene/rider parsing or scene-generation change is visible to users.
- The same feature requires both a user workflow explanation and an update to parsing/placement technical rules.
- UI labels/help examples must change to stay consistent with updated parser behavior.

If the change is purely internal and does not alter user-visible behavior, update only `docs/text_to_scene_rules.md` when the technical contract changes.

### PR self-check (required before merge)

- [ ] I reviewed this checklist and updated `help.md` where user-facing behavior changed.
- [ ] I updated `docs/text_to_scene_rules.md` where parser/scene-generation rules changed.
- [ ] I updated both files in the same PR when both criteria applied.
- [ ] I added cross-links instead of duplicating long rule sections.

## Assets and Media

- Store screenshots and diagrams under `docs/` or `resources/` as appropriate.
- Embed media only where it adds clear instructional value.
- Compress images before committing to keep repository size manageable.

## License Reference

- Keep license details in `LICENSE.txt` as the single source of truth.
- Reference the license from `README.md` and any documentation that needs legal context.
