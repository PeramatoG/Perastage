# Perastage 1.8.0

<!--
RELEASE NOTES — WORK IN PROGRESS

This is the living draft for the next Perastage release.

Maintenance rules for Codex and contributors:
- Update this file in the same pull request as each meaningful user-facing change, before merge.
- Describe only implemented and adequately verified behavior; do not announce plans or unsupported capabilities.
- A pull request may include its own new entry while under review; main contains only merged entries.
- For internal-only changes or justified exceptions, explain why no entry is necessary in the PR description.
- Focus on benefits and observable behavior, using clear, concise English.
- Group related changes and avoid duplicate entries or detailed implementation histories.
- Do not repeat features already announced in Perastage 1.7.0.
- Preserve the overall structure and existing verified entries.
- Before publication, validate all claims, remove empty sections and placeholders, and check release assets.
-->

<!-- Replace this placeholder with a short release overview after the feature set is finalized. -->

## Highlights

- More reliable MVR import and export, with improved preservation of standard scene data, assignments, references, and transforms.
- Smarter GDTF matching and DMX mode selection during MVR import, with clearer manual choices.
- New Fixture Label Editor with a shared label presentation across the 2D viewer, Layout captures, and vector PDF output.
- More reliable Layout Undo/Redo, including page operations, element editing, and cancelled interactions.
- Canonical MVR export by default, with compatibility output available through an explicit choice.
- Easier platform downloads and installation guidance on the Perastage website.

## MVR / GDTF

- Repair repeated imported object identities consistently across editable objects and retained source structure, preserving placements, Class assignments and shared model bytes through project save/reopen and canonical MVR export. Empty optional GDTF fields no longer block saving; incomplete optional Truss/Support bindings are omitted with specific warnings.

- Project saving and canonical MVR export now retain validated source-only fixture fields such as shadow settings and custom commands. Failed saves keep the previous file and session path, with complete diagnostic details available in the error dialog.

- The GDTF download window now stays open after a completed MVR import queue until acknowledged. A preference can restore automatic closing.

- Automatic GDTF matching better distinguishes suitable fixture models from ambiguous candidates. Matching DMX modes can be inferred from the imported mode name or unambiguous channel-count evidence, including after manual GDTF selection; uncertain matches retain the original profile for review.

- Direct-geometry MVR FocusPoints and their Fixture references now survive supported import, project save/reopen, and export workflows. Additional Fixture properties, including Function, inversion, custom identifiers, and unchanged authored CIE color, are preserved.

- Standard MVR Classes and Classing references are retained for supported scene objects, including definitions and assignments that do not belong to the same layer.

- Nested MVR Symdefs, Symbol instances and geometry placements now survive project saves, merges and canonical exports, including model textures and unused definitions. VideoScreen/Projector data, file media and parametric children are retained even without an editor. Editing an instance’s geometry retains its original definition. Unavailable resources and invalid source relationships receive precise recovery warnings.

- Recoverable MVR import defects no longer veto supported project saves and exports: canonical publication repairs identities and invalid values, preserves wide integer labels, and reports specific omitted or substituted content. Recovery disclosures survive reopening; real write failures preserve the previous file.

- Authored MVR fixture identifiers and UnitNumber presence are preserved without automatically renumbering valid imported values. High-precision matrices are retained through supported canonical saves and edits.

- Standalone MVR export now replaces the destination only after the complete archive closes successfully; failed writes retain the previous file.

- Normal MVR export now uses the same canonical representation as project saving. An explicit option is required to use a preferred compatibility representation for an individual export.

- Truss nodes now preserve supported DMX and network assignments through project save/reopen and MVR export, retaining their original GDTF interfaces.

- Support nodes now retain DMX and network assignments through project saves and MVR export; Fixture-to-Support conversion preserves compatible data and refuses conversions that would discard authored information.

- Fixture network interface assignments now survive MVR import, project save/reopen and export, including alongside multiple DMX breaks and through patch edits.

- MVR fixtures now retain all DMX-break assignments through project saves and exports; editing the primary patch leaves other breaks unchanged.

- MVR layer elevations now survive import, project save/reopen and MVR export, keeping child objects correctly placed through grouping, merging and Undo/Redo.

<!--
Document further completed MVR/GDTF improvements here. Preservation applies only
within the supported and validated scope; do not claim universal losslessness,
full MVR/GDTF conformance, or interoperability with every third-party application.
-->

## Layout, Symbols and Plotting

- Layout Legends now use the same bundled font, line wrapping, row geometry and symbol columns in the editable preview and vector PDF. Enabled Side symbols remain vector geometry, independent settings refresh reliably, and unsupported glyphs are identified in PDF completion details.

- A new **Fixture Label Editor** lets users adjust Name, ID, and DMX Address labels, including visibility, font weight, size, colors, outlines, alignment, anchors, and position. It supports project defaults, per-fixture overrides, and preview dragging.

- Edited fixture labels share their positioned presentation between the interactive 2D viewer, Layout view captures, and vector PDF. Legacy name wrapping and small-text zoom appearance are retained more consistently.

- Layout page operations and element edits now participate reliably in project Undo/Redo. Selection-only actions, unchanged edits, and cancelled moves or resizes no longer create unwanted history entries; image resources are retained across undo and redo.

<!--
Further work may include physical drawing scales, annotations, dimensions,
reusable label styles, and shared typography for free text, legends and tables.
Do not list these as delivered until implemented and verified.
-->

## CLI and External Integrations

<!--
Add completed, user-facing CLI, OSC or external-integration changes here.
Clearly identify experimental functionality; do not repeat 1.7.0 features.
-->

## Performance and Usability

- Project saving now reports more actionable errors when MVR data or resources cannot be preserved. The saving overlay closes before displaying the error, so the application does not remain blocked behind the dialog.

<!-- Add further verified user-visible performance or usability changes here. -->

## Compatibility and Stability

- Malformed 3DS models now fail cleanly when chunk data is truncated or face indices are invalid, improving stability during model loading and previews.

- Improved macOS 15 build compatibility for decimal-number parsing used by GDTF electrical information and SVG geometry.

- Unsupported or ambiguous MVR changes are rejected more safely, helping prevent accidental data loss during affected conversions, imports, project saves, and exports.

<!-- Add other verified cross-platform and stability fixes here without duplicating detailed MVR entries. -->

## Documentation and Help

- Updated user guidance for the GDTF import queue, fixture labels, and Layout editing.

- Platform-specific installation guidance and direct-download information are available on the Perastage website.

<!-- Add further verified documentation and Help changes here. -->

## Downloads

The Perastage website offers direct links to supported release packages, with installation guidance and a fallback to GitHub Releases. The Windows download area also offers a Microsoft Store option alongside the standalone installer.

Choose the appropriate official package from the release page or the assets attached to this GitHub release.

<!--
Before publication:
- Verify the available release artifacts and platform/architecture support.
- Confirm the macOS 15 compatibility build if provided.
- Confirm the Microsoft Store listing and all download links.
- Verify installation instructions and whether the experimental CLI is bundled.
-->

Documentation and installation instructions:
https://perastage.luismaperamato.com/

Issues and feedback:
https://github.com/PeramatoG/Perastage/issues
