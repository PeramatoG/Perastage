# Changes Since Beta 0.1.0

This document tracks major functional expansion from the beta 0.1.0 era to the current stable release line. It focuses on user-visible capability growth and workflow hardening.

## Release Position

- Beta baseline: `0.1.0` (early workflow coverage, limited production guarantees).
- Current release line: `1.0.0` (core workflows stabilized and documented).

## Major Functional Growth

## 1) MVR/Open workflow maturity

### Then (beta)

- Import existed but behavior differed more across entry points.
- Open semantics could be less deterministic for unsaved work and scene resets.

### Now

- MVR import/export flow is a first-class production workflow.
- `.mvr` opening from menu and OS associations follows consistent save-check and reset+import behavior.
- Progress and UI-disabling patterns are aligned across entry points.

## 2) GDTF integration and export robustness

### Then (beta)

- Basic fixture mapping and GDTF usage existed with limited policy formalization.

### Now

- Export paths use archive-relative forward-slash references.
- Deterministic collision renaming prevents ambiguous duplicated GDTF filenames.
- `description.xml` mutation behavior and compatibility handling is formally documented.
- Fixture ID assignment across exported parametric objects is normalized and globally unique.

## 3) Rider/text-to-scene parser expansion

### Then (beta)

- Text import existed in early form with narrower rule coverage.

### Now

- Text and PDF rider pipelines are both supported.
- Apply-filter-first workflow improves pre-creation control.
- Placement and hang-token parsing covers broader real-world rider syntax.
- Parser behavior has an explicit rule contract in `docs/text_to_scene_rules.md`.

## 4) Dictionary portability and asset safety

### Then (beta)

- Dictionary workflows were narrower and less portable.

### Now

- Three portability levels are supported (reference JSON, asset-copy snapshot, portable ZIP bundle).
- Preflight validation and missing-reference reporting are integrated.
- Collision handling supports rename/overwrite/cancel policy selection.

## 5) Layout and print pipeline depth

### Then (beta)

- Layout and print flows were more limited and less integrated.

### Now

- Multi-page layout authoring supports views, legends, event tables, text, and images.
- PDF output is suitable for complete documentation sheet generation.
- Print workflows include tables and layout contexts, with explicit Debug/Release gating where needed.

## 6) Viewer and editing workflow improvements

### Then (beta)

- Core 2D/3D viewing existed with fewer production controls.

### Now

- 3D and 2D viewers are integrated with layer, selection, and table workflows.
- Batch editing helpers in tables support interpolation and relative updates.
- Console command workflows and selection commands support fast operational adjustments.

## 7) Platform integration and packaging hardening

### Then (beta)

- Packaging and file association behavior was less standardized.

### Now

- Windows Inno Setup path is the official installer flow.
- Linux desktop and MIME integration are defined in install layout.
- macOS document type metadata supports Finder file routing.

## 8) Documentation and policy formalization

### Then (beta)

- Knowledge was concentrated in long-form mixed README content.

### Now

- README is a concise landing page.
- Deep workflows are distributed across focused docs.
- Specialized behavioral policies exist for parser rules, GDTF mutation, shortcuts, architecture, and build conventions.

## Areas Still Evolving

- Large-scene performance and selected advanced tooling remain active improvement areas.
- Some utilities remain build-gated or workflow-specific by design.

## Related Documents

- [Feature overview](features.md)
- [Build and dependency guide](build.md)
- [Packaging and platform integration](packaging.md)
- [Text-to-scene rules](text_to_scene_rules.md)
- [GDTF mutation policy](gdtf_mutation_policy.md)
