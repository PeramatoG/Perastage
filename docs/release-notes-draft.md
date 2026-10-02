# Perastage v1.7.0 Release Notes

Changes since **v1.6.0**.

## Highlights

## New features and workflow improvements

- Added the first local live connection between `perastage-cli` and a running
  Perastage instance. Supported Console commands now operate on the active
  project with normal validation, Undo, dirty-state, selection, and refresh
  behavior, while summary and selection queries return structured results over
  a bounded loopback-only connection. Shutdown now safely cancels pending GUI
  dispatches, and live operation failures use the documented general command
  failure exit status consistently across platforms.

- Added a headless `perastage-cli scene` workflow for applying the existing
  fixture/truss selection and transform commands to an explicitly supplied MVR
  and publishing a canonical result to a separate output file. Sources are
  never modified, existing outputs require explicit overwrite permission, and
  failed command sequences publish no partial result. Machine-readable failures
  retain semantic command diagnostics, while application exports continue to
  use their configured fixture-library fallback without exposing that library
  to isolated CLI workflows. Exporter regression coverage now explicitly
  protects both sides of that resource-ownership boundary. Neutral GDTF naming
  and layer validation now link independently from application dictionary,
  project-state, and Undo services in headless builds. Source ownership checks
  also reject filesystem aliases and accept case-insensitive MVR extensions,
  with consistent headless test linkage and resolved Git Bash policy execution
  across supported platforms.

- Added a native, read-only MVR / GDTF Inspector workspace alongside the 3D,
  2D, and Layout Mode views. It now opens on a canonical snapshot of the active
  project, supports explicit refresh and external MVR/GDTF files, provides
  hierarchical package and scene navigation, structured issue summaries, and
  exact syntax-highlighted XML with folding and wrapping search. Embedded GDTFs
  can be inspected without extraction, package paths are available from their
  context menu, and the workflow remains non-mutating and offline. Nested MVR
  ChildList relationships remain intact in the scene tree, embedded GDTF
  resources keep their normal classifications, and restored Inspector layouts
  initialize the current-project snapshot only when visible.
- Expanded the read-only GDTF Inspector with dedicated FixtureType overview,
  DMX mode and channel hierarchy, and wheel/resource pages. Standalone and
  embedded GDTFs now expose authored metadata, physical ranges, attributes,
  channel functions and sets, filters, colors, and geometry references without
  permanent extraction, downloads, or an editing workflow. XML folding markers
  now respond to clicks, line numbers adapt to each document, Inspector tabs
  restore by stable page identity, and issue and detail punctuation renders
  consistently across supported platforms.

## Compatibility, stability, and performance

- Restored broad Linux AppImage compatibility by building and validating the
  complete package against Ubuntu 22.04, glibc 2.35, and GCC 11, with a
  packaged-application startup check before release. The fixed ABI audit now
  reflects Ubuntu 22.04's supported C++ runtime independently of the selected
  compiler version, and startup validation identifies the real visible main
  window independently of the AppImage launcher process.

- Added a GUI-independent semantic command foundation with typed requests,
  structured diagnostics, efficient atomic scene mutation rollback, and
  deterministic machine-readable results for future command workflows.
- Added a GUI-independent, read-only scene query foundation with deterministic
  object, selection, layer, group, and fixture patch results, including one
  canonical patch-address interpretation, for future automation and headless
  integrations.
- Added deterministic capability discovery for existing semantic Command and
  Query operations, including argument types, effect classification, and
  truthful frontend availability. The development CLI displays the catalog in
  human-readable or versioned JSON form without loading or changing a project.
- Migrated grouping, ungrouping, Fixture-to-Hoist conversion, and same-model
  SceneObject-to-Truss conversion to the shared semantic command layer. These
  actions now use one atomic Undo operation, preserve stable object identities,
  avoid empty history entries when an operation cannot make a change, and
  prevent SceneObject conversion from replacing an existing truss that has a
  conflicting identifier. Invalid grouping requests with repeated objects are
  rejected before changing the project.

- Made the read-only MVR / GDTF Inspector responsive on production-scale files
  with managed background inspection, latest-selection result safety, scalable
  model-backed package and Scene navigation, resizable remembered workspace
  panes, bounded large-XML loading, and on-demand image, text, model, and nested
  GDTF previews. Preview resources remain immutable and temporary model data is
  removed automatically. Source loading now remains authoritative over stale
  resource interactions, current-project archive creation and GLB/3DS geometry
  preparation run in managed background work, and returning from embedded GDTF
  inspection restores the exact parent package and preview identity.

- Strengthened read-only Inspector reliability with complete deterministic parity, malformed-container, Unicode resource, diagnostic-separation, and non-mutation characterization, and made Inspector coverage explicitly visible in the informational Core/MVR report.

- Improved the Inspector with explicit XML fold and unfold actions and a
  persistent, resizable Preview pane below the details area. XML fold markers
  and document-wide folding now work consistently, while embedded GDTFs retain
  their bounded single-click 3D preview.

- Improved Inspector resource presentation so selected SVGs show exact,
  syntax-highlighted XML in Source and an in-memory rendered image in Preview
  from one bounded package read. SVG previews now preserve their authored
  viewport proportions and remain centered in portrait or landscape preview
  areas across supported platforms while retaining transparency and explicitly
  authored white fills.
  Other XML and plain-text resources now use the
  Source pane without duplicate Preview text, while image, model, unsupported,
  and failed selections reliably restore the authoritative package XML.

## Important fixes

- Restored GCC 11 compilation of the asynchronous update check while
  preserving its existing background-check and busy-dialog behavior.

- Restored macOS 15 build compatibility for persisted Inspector splitter
  preferences while keeping their representation locale-independent. Manually
  started compatibility builds now validate the branch selected by the maintainer
  unless an explicit source override is supplied.

- Prevented a Windows crash when activating an embedded GDTF by deferring the
  source transition until the package-tree activation callback has returned,
  avoiding destructive model replacement during native event dispatch. Nested
  inspection now keeps the parent MVR visible until the embedded document is
  ready, restores parent interaction after failures, and lays out Back
  navigation only after a successful transition. The nested-open bound now
  shares Inspection Core's resource-read safety limit, allowing valid embedded
  fixtures to open instead of being rejected before reading. Inspector tab
  ownership now remains fixed while switching between MVR and GDTF, preventing
  native Windows notebook assertions during transition repainting.

- Kept fixture previews reliable when switching between OpenGL views by
  rebinding the Preview canvas context before every render. Selecting the
  primary MVR or GDTF XML document no longer duplicates it in Preview, and a
  one-time Inspector layout preference migration restores the intended
  two-thirds XML and one-third details proportions without overriding later
  user adjustments.

- Corrected GDTF inspection so the standard `FTPresets` section is no longer
  reported as unknown, while canonical publication now removes the known
  legacy Perastage `Editor` marker, records the cleanup as a standard revision,
  and continues to report the unmodified source's standards error. Explicit
  fixture exports and Perastage-owned project and library derivatives now use
  the same failure-safe canonical publication policy without modifying external
  sources or discarding unknown vendor metadata, preserve same-file export
  behavior, and repair known placeholder fixture identities to the same stable
  identifier on every supported platform only in published copies. The
  development CLI now labels semantic read status explicitly so it cannot be
  mistaken for schema-validation status.

- Corrected Inspector and development CLI handling of large XML package
  resources, aligned complete-report validation outcomes with CLI exit codes,
  and removed duplicate human-facing diagnostic rows without discarding
  structured validation provenance. MVR 1.5 inspection now avoids misleading
  MVR 1.6 schema errors, reports semantic findings against verified official
  MVR 1.5 specification provenance, and keeps unsupported-version provenance
  explicit. Read-only scene snapshots now include focus points, video screens,
  and projectors with their deterministic packaged-resource references and
  complete direct authored child hierarchy across standard MVR 1.6 ChildList
  owners, without attributing descendant resources to their neutral parent.

- Restored fixture-category fallback diagnostics during normal MVR imports.

- Restored detailed MVR import diagnostics for matrix contexts and examples, per-Symdef truss usage, GDTF resolution, dictionary state, and fixture-category fallback reasons.

- Restored progress reporting for MVR fixture-category enrichment and final GDTF mode resolution.

- Extended read-only MVR inspection summaries with parsed Position and Symdef structures already retained by the production reader.

- Restored application import logging from the shared MVR reader while keeping standalone inspection independent of the application logger.
- Corrected read-only MVR inspection for recovered node identities, exact authored layer names (including unnamed layers), duplicate layers, direct-child hierarchy, nested groups, and complete deterministic Symdef resource inventories without changing imported scene data.

- Kept standalone MVR inspection independent of application GDTF enrichment, while restoring exact cached fixture-category provenance, final mode compatibility resolution, and default-layer normalization for normal imports.

- Preserved fixture-category dictionary updates and the complete GDTF Share conflict-download workflow when importing MVR files through the shared inspection reader.

- Restored bounded filesystem ZIP inventory reads and removed redundant full-file loading during filesystem MVR inspection.

- Prevented Windows debug assertions during rapid or interrupted mouse drags by balancing capture ownership across viewers, layout editing, previews, and tables.

- Fixed sortable data-table columns to use semantic numeric and natural ordering instead of lexicographic string ordering.

- Restored cross-platform Debug compilation of the Dictionary Editor after its snapshot-service architecture update.

- Restored smooth, current-pointer object dragging in the 2D and 3D viewers while keeping scene visibility stable throughout each drag.
- Fixed Layout Viewer 2D View dragging and resizing so Legend refreshes cannot redirect edits to a Legend.
- Restored Windows Debug compilation of the Layout Viewer after its selection architecture update, including View2D frame lookup integration.

## Technical and packaging changes

- Completed the embedded Console frontend boundary: reusable text-command
  ordering and execution now live in GUI-independent Command infrastructure,
  while the Console retains its established output, selection presentation,
  viewer refresh, and Undo behavior. Focused dependency guards and ordered
  partial-failure coverage protect this separation across future changes.

- Strengthened capability discovery contract coverage for exact operation
  inventory, argument types, frontend exposure, JSON structure, CLI output,
  cross-platform compatibility, and dependency boundaries. Capability discovery
  now consumes neutral operation identifiers without linking scene mutation or
  transaction infrastructure.

- Moved Console fixture and truss selection semantics into the shared,
  GUI-independent command layer with stable UUID identities, deterministic
  numeric-ID diagnostics, deterministic removal of all selected fixtures that
  share an ID, preserved chained-command behavior, and accurate mutation
  reporting when a selection clear cannot be published.

- Kept Linux build prerequisites centrally maintained and non-interactive on
  CI runners and compatibility containers, including isolated Python build
  environments and pinned build tools required by dependencies, with aligned
  packaging policy validation and consistent project generation.

- Moved Console position and rotation execution into a reusable,
  GUI-independent semantic command boundary with transactional Undo, no-op,
  and rollback behavior while preserving existing Console syntax and results.
  The human-text parser remains independently reusable, and transform-space
  regression coverage now follows the established yaw, pitch, and roll
  convention on every supported platform. Read-only Core consumers remain
  independent from generic scene-node mutation operations.

- Moved the Command Bar's human-text grammar into a reusable, GUI-independent
  parser while preserving current Console syntax and keeping selection policy
  and scene execution behavior unchanged in the application layer. Console
  syntax errors retain specific, ordered technical diagnostics, and automated
  architecture checks protect the parser boundary on every supported platform.

- Reduced clean test-build work by reusing identical production implementation objects across focused MVR, Rider importer, and GDTF loader test executables without changing the test inventory or coverage.

- Added reproducible dependency metadata, policy-aware local and CI version
  reporting, triplet-safe validation, and a safe quarterly draft-review
  workflow with stronger checkout and policy safeguards that do not pin
  changeable port revisions in tests. Local Windows reporting now reads
  metadata from the standard classic-vcpkg installation-root layout.
- Completed the supported Linux setup package lists for XML schema discovery
  and native secure credential storage.

- Made Inspector policy tests use the repository's portable Python test-tool
  launcher so they run consistently on Windows, macOS, and Linux CI hosts.

- Documented and regression-tested the development CLI automation contract for versioned JSON, exit codes, stream routing, Unicode paths, and structured diagnostic locations, while explicitly keeping human-readable presentation outside the machine-compatibility promise. The CLI remains unshipped.

- Added a headless `inspect` command to the development CLI for GDTF and MVR summaries, package and resource inventories, diagnostics, exact retained XML, and structured JSON, with deterministic exit codes, Unicode-safe native Windows paths, complete structured GDTF document facts, clean data/process stream separation, and layered Core serialization boundaries that keep neutral results independent from full format reports, and explicit cross-platform UTF-8 report verification. The CLI remains unshipped.

- Added a dedicated headless command-line executable for development and cross-platform automation, with stable help and version output, deterministic usage errors, and strengthened automated separation from GUI startup, application state, localization, and packaging; it is not included in shipped packages yet.

- Added a safe, read-only package resource inspection boundary with bounded raw reads, conservative file-kind identification, unchanged UTF-8 text previews, and embedded GDTF inspection through the established standalone path. Image and model decoding remains outside the semantic inspection service.
- Improved package resource inspection reliability across platforms, including deterministic owned-byte reads and consistent GDTF compatibility handling.
- Ensured package browsing binds each selected ZIP entry to its exact payload even when archive directory ordering differs, while retaining bounded signature detection for large resources.

- Added offline, read-only validation with exact pinned official GDTF 1.2 and MVR 1.6 schemas, while keeping well-formedness, structural standards, version-specific semantics, and compatibility results separate.
- Improved compatibility with locally managed libxml2 versions, added earlier Windows dependency diagnostics for classic vcpkg builds, and included the corresponding runtime legal notice.

- Added a standalone, read-only MVR inspection service backed by the same single parser as application imports, exposing equivalent filesystem and in-memory package inventory, original scene XML, deterministic scene hierarchy, preserved foreign provider data, embedded and missing resource references, and structured diagnostics without applying the scene or downloading missing resources.

- Added a read-only, GUI-independent GDTF inspection service that exposes package contents, fixture metadata, DMX modes, wheel references, raw XML, and structured compatibility diagnostics without modifying or canonicalizing source files, while keeping its shared reader dependency limited to wxWidgets base/archive facilities.

- Added a framework-independent, read-only package inventory boundary for supported GDTF and MVR files, including Unicode-preserving entry metadata, shared bounded ZIP directory checks, and safe rejection of malformed, unsupported, or traversal-style packages without extracting or importing package contents.
- Hardened resource browsing for portable nested and Unicode ZIP paths, including valid UTF-8 names without an archive encoding flag, and capped lightweight media sniffing independently of caller input.

- Added a deterministic, versioned JSON boundary for neutral inspection results, with cross-platform UTF-8 verification for automation without coupling inspection semantics to command-line or graphical presentation.

- Introduced a minimal reusable non-GUI inspection library shared by the application and focused architecture tests, with strict ownership checks that preserve one implementation for future inspection frontends without migrating unrelated Core code.
- Corrected MVR reader build ownership so reusable Core and scene-model implementations remain registered by their owning modules and are linked without duplicate compilation.

- Established a cross-platform architecture guard for the GUI-independent, read-only inspection contract used by future file-analysis tools and frontends.

- Added a concise maintainer runbook that routes routine operations, recovery, and external-service dependencies to their canonical repository procedures.

- Consolidated maintainer documentation around current canonical sources and focused subsystem contracts, removing obsolete audit and checkpoint journals.

- Replaced the dated code-health review with a living, policy-linked maintenance contract.

- Separated hoist table row-to-scene editing into a focused service while preserving inherited field sources, dummy profiles, automatic loads, transforms, undo, and view-refresh behavior.

- Separated truss table row-to-scene editing and equal-type physical-property synchronization into a focused service while preserving undo, resource-reference, load-recalculation, and view-refresh behavior.

- Separated reusable dictionary JSON snapshot serialization and reference-path validation from the Dictionary Editor while preserving existing snapshot and portable bundle behavior.

- Moved GDTF document mutation and atomic archive publication into a reusable Core component while preserving existing GDTF output, diagnostics, compatibility APIs, and Viewer3D cache refresh behavior.

- Separated the GDTF Share catalog, authentication, search, and download interface workflow from the main-window menu implementation while preserving existing behavior.

- Improved Windows Debug CI dependency performance by separating ABI-protected vcpkg binary archives from mutable installation trees, broadening safe cache reuse across runner updates, and adding cache-source and installation-time diagnostics.

- Separated reusable mouse and keyboard camera navigation, pointer-drag activation state, and grouped selection decisions from the wxWidgets/OpenGL panel, with production-path interaction coverage, while preserving existing viewport behavior.
- Separated reusable 3D viewer hover, refresh, interaction-settle, resource-sync cadence, and performance telemetry decisions from the wxWidgets/OpenGL panel while preserving existing viewport behavior.
- Encapsulated reusable 3D manipulation, continuous-placement, line-point selection, and measurement lifecycles behind guarded GUI-independent boundaries while preserving existing viewport behavior.
- Corrected cross-platform Viewer3D camera-input integration with the extracted interaction sessions.
- Isolated reusable 2D viewport navigation, rectangle selection, and selection-drag session state behind a GUI-independent boundary, with portable lifecycle checks that preserve existing interaction behavior and cross-platform compilation.
- Isolated reusable 2D hover scheduling, interaction settling, and picking-cache decisions behind a GUI-independent boundary while preserving existing viewport responsiveness and picking behavior.
- Isolated reusable 2D continuous-placement and line-point-selection lifecycle state behind GUI-independent boundaries while preserving existing tool behavior.
- Separated reusable 2D navigation, typed selection, and hover-routing decisions from the wxWidgets/OpenGL panel while preserving established viewport interaction behavior.
- Completed the 2D viewer architecture split by isolating reusable frame decisions, ruler state, and layout-overlay geometry while retaining OpenGL and window lifecycle ownership in the panel.
- Isolated layout-editor pointer sessions, frame-handle hit testing, and frame move/resize decisions behind a GUI-independent boundary while preserving existing image proportions, grid snapping, and undo behavior.
- Isolated Layout Viewer selection identity, default selection, and stable Z-order decisions behind a GUI-independent boundary while preserving established editing and stacking behavior.
- Isolated Layout Viewer viewport navigation, fit, geometry, and safe-zoom decisions behind a GUI-independent boundary while preserving established viewport behavior.

- Separated MVR archive creation and file/buffer transport from scene export orchestration while preserving canonical cross-platform package contents, diagnostics, and Unicode-safe filesystem paths.

- Separated MVR resource selection, dependency collection, and deterministic package planning from export orchestration while preserving existing archive compatibility.

- Separated MVR XML writing from export orchestration by standard node and Perastage extension responsibility while preserving package compatibility.

- Separated MVR export scene validation and deterministic preparation from XML serialization and package writing while preserving existing export behavior and diagnostics.

- Isolated MVR identity normalization, legacy reference remapping, and post-parse cross-reference validation behind a reusable import component while preserving deterministic recovery and structured diagnostics.
- Separated reusable MVR parsing results from active-project replacement while preserving established project-reset and result-inspection behavior.
- Preserved Position names exactly as authored, including surrounding whitespace, when importing MVR files.

- Isolated MVR GDTF and scene-resource resolution, metadata access, and caching behind a GUI-independent import service while preserving deterministic resource-name spelling, unresolved support references, mode selection, and fixture-category handling across supported platforms.

- Separated logical MVR scene-node and hierarchy reading into an independent compiled module while preserving existing fixture, rigging, geometry, layer, and compatibility behavior.
- Preserved native Unicode filesystem paths across MVR scene-resource resolution on all supported platforms.
- Improved Windows compiler reliability for the extracted MVR scene-node reader.

- Separated MVR package acquisition and archive safety handling from semantic scene parsing to improve importer maintainability without changing supported MVR behavior.
- Corrected the informational Core/MVR coverage workflow diagnostics, dependency logging, and Linux locale preparation, with CI policy checks that prevent incomplete test environments.
- Established informational Linux coverage reporting for Core and MVR production code and expanded deterministic MVR import/export characterization ahead of future internal refactoring.
- Reconciled maintainer documentation with the active main-branch ruleset and dedicated release-App configuration.

- Added a cross-platform repository hygiene guard that blocks accidentally tracked build artifacts and unexpectedly large files while preserving the bounded bundled GDTF library as normal Git assets.
- Added an automated, cross-platform source-size guardrail that prevents existing C/C++ hotspots from growing beyond reviewable baselines and limits new source files to a maintainable default size.
- Validated the native Linux, Windows, WSL, and Apple Silicon macOS clean-checkout developer workflows, corrected Linux/WSL prerequisite installation order and Debug test dependencies, prevented Windows-only PowerShell tests from registering on Linux hosts, completed Linux system dependency and locale setup, fixed external mDNS header discovery, corrected Windows wxWidgets secure-store and Debug test-tool validation, made restricted-path policy tests use canonical directories across macOS and Windows environments, and kept local test artifacts out of the source tree.
- Prevented compatible macOS dependency caches from being discarded because binary contents were misread as SDK metadata, and migrated CI away from an immutable stale cache snapshot so repaired dependencies can persist across runs.
- Added a reproducible repository-structure baseline and policy validation to protect current module and build ownership during future organization work.
- Strengthened repository policy checks to protect third-party ownership, top-level module ownership, portable shared configuration, and explicit CMake source registration.
- Improved cross-platform reliability of the repository policy regression fixtures without weakening machine-specific path detection.
- Clarified Core ownership of the shared viewport interaction preference policy and removed its temporary repository-root compatibility exception.
- Established the shared CMake presets as the canonical local build configuration and documented optional, untracked developer overrides across supported platforms.
- Made the Windows classic-vcpkg workflow portable and reliable in Visual Studio through explicit or user-wide external checkout discovery, while ignoring the IDE's injected bundled dependency tree, keeping cross-platform validation reliable, and removing redundant legacy configuration files.
- Gave the scene-model module explicit ownership of its application source registration while preserving existing build behavior.
- Completed explicit CMake source ownership across all application modules and strengthened cross-platform, harness-aware repository checks against architecture drift.
- Moved application dependency discovery into a dedicated build module while preserving existing package-manager and platform behavior.
- Moved localization build configuration into a dedicated module while preserving existing catalog and platform behavior.
- Moved build-tree runtime asset staging into a dedicated module while preserving existing cross-platform resource layouts.
- Moved install-tree rules and packaging staging into a dedicated build module while preserving existing cross-platform installation layouts.
- Moved CPack compatibility configuration into a dedicated build module while preserving existing package metadata and installer behavior.
- Completed the build-system modularization by isolating platform target configuration and simplifying the root CMake file to project orchestration.
- Localized application include-directory ownership to feature modules while retaining shared and dependency-provided build requirements at the project level.
- Established a cross-platform, machine-checked contract for current internal module dependency directions, making accidental new coupling visible with consistent diagnostics on every supported operating system.
- Kept the Linux/WSL setup command stable while moving its detailed bootstrap workflow into a dedicated platform script with portable cross-platform compatibility checks.
- Kept the Windows setup command stable while moving Visual Studio, dependency validation, and build orchestration into dedicated platform scripts and aligning setup documentation.
- Moved application startup and lifecycle composition into a dedicated module while preserving launch, open-file, localization, diagnostics, splash, and shutdown behavior, leaving the root entry point minimal.
- Reconciled developer documentation with the current modular repository layout and clarified the authoritative references for architecture, paths, and structural policy.
- Enforced repository architecture boundaries for root entry points and source-module registration, and prevented developer-local build, preset, IDE, and machine-path configuration from being committed.
- Completed a test-backed regression audit of repository organization, build ownership, resource staging, packaging structure, documentation, and supported-platform boundaries.
- Completed the repository-organization documentation review and reconciled the merged CI and hosted packaging evidence while preserving an explicit distinction between verified builds, non-publishing dry runs, and real releases.

## Downloads and installation

Choose the package that matches your operating system:

| Operating system | Download |
|---|---|
| **Windows 64-bit** | `Perastage_1.6.0_Setup.exe` |
| **macOS 15 — Apple Silicon** | `Perastage-1.6.0-macOS15-arm64.dmg` |
| **macOS 26 — Apple Silicon** | `Perastage-1.6.0-macOS26-arm64.dmg` |
| **Linux x86-64** | `Perastage-1.6.0-x86_64.AppImage` |
| **Arch Linux x86-64** | `Perastage-1.6.0-arch-x86_64.pkg.tar.zst` |

> **Do not download `Perastage-1.6.0-Debug-Symbols-Developers-Only.zip` unless it is requested for crash analysis or you specifically need the developer debug information.**
>
> This archive is not required to install or run Perastage.

### Windows

Windows SmartScreen may warn that the application is from an unknown publisher because Perastage is an independent open-source project and is not currently code-signed.

Select **More info**, then **Run anyway** to continue.

### macOS

Perastage is not currently notarized by Apple. If macOS blocks the first launch:

1. Open **System Settings → Privacy & Security**.
2. Select **Open Anyway** for Perastage.
3. Confirm that you want to open the application.

### Linux

The AppImage may need executable permission:

```bash
chmod +x Perastage-1.6.0-x86_64.AppImage
```

### Arch Linux

Install the package with:

```bash
sudo pacman -U Perastage-1.6.0-arch-x86_64.pkg.tar.zst
```

## Need help?

Please open a GitHub issue if you encounter a problem. Include the Perastage version, operating system, clear steps to reproduce the issue, and a diagnostic report from the **Help** menu whenever possible.

You can contact the project at **perastage.app@gmail.com**.
