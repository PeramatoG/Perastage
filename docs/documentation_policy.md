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

## Cross-Linking Rules

- Avoid duplicating large sections across files.
- Link related guides using relative links.
- When adding a new major workflow, add one short bullet to `README.md` Highlights and place details in the appropriate `docs/*.md` file.

## Assets and Media

- Store screenshots and diagrams under `docs/` or `resources/` as appropriate.
- Embed media only where it adds clear instructional value.
- Compress images before committing to keep repository size manageable.

## License Reference

- Keep license details in `LICENSE.txt` as the single source of truth.
- Reference the license from `README.md` and any documentation that needs legal context.
