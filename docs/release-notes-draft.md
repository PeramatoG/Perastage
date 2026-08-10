# Perastage v1.6.0 Release Notes

Changes since **v1.5.0**.

## Highlights

## New features and workflow improvements

- Layout PDF completion messages now summarize how many elements used generated
  Perastage symbols and how many used rendered fallbacks, making it easy to
  verify symbol usage without comparing PDF file sizes.

- Improved truss Magnet placement by preferring connector points declared in
  GDTF files, using terminal points for clearly straight trusses, and retaining
  conservative face-center snapping for ambiguous truss shapes and groups.
  Project-relative connector resources are cached safely, while 3D assembly
  now acquires visually overlapping endpoints with a DPI-independent screen
  aperture for more predictable perspective-view placement. Truss groups now
  expose their real unoccupied member connectors, and straight extensions
  choose the non-overlapping end when approaching an assembly from either side.

- Added **Selection & Movement** preferences for choosing, independently for
  fixtures, trusses, supports/hoists, and scene objects, whether mouse,
  command-bar, and Magnet transforms move an exact grouped object or its
  highest containing group. Table edits remain exact and project/MVR hierarchy
  data is unchanged by the preference.

## Compatibility, stability, and performance

- Fixture symbols now distinguish GDTF ownership from symbol availability: valid
  external four-view symbols are used directly, while malformed or empty SVG views
  are rejected before publication. Runtime preparation work coalesces by physical
  resource and exact mode, captures cooperatively between interface updates, remains
  fallback-first, and is protected against stale project lifetimes without adding
  Save or export waits. Incremental captures now use one immutable scene input from
  warm-up through all four views, and closing a manual preview without applying no
  longer suppresses later automatic preparation. Automatic processing now runs on a
  managed worker and advances through low-priority idle slices, while completed
  publications immediately resynchronize 3D fixture resources to avoid temporary
  fallback cubes.

- Fixture-symbol updates now build and validate an isolated working GDTF before
  atomically publishing the project-owned derivative. Failed updates leave existing
  fixture references and files intact, shared fixtures remain on one derivative, and
  optional library synchronization can no longer undo a valid project result.

- Fixture symbol capture now renders from a capture-only scene snapshot without
  replacing project scene containers or temporarily changing the live fixture. Capture
  data is limited to each offscreen render step so interactive viewers cannot observe it.
  Automatic capture now completes every offscreen scene-replacement transition
  before restoring the active project, preventing distorted non-top symbols and
  ensuring layout 2D views and printed layouts retain fallback or stored-symbol
  geometry while preparation progresses. Each yielded symbol view now refreshes
  its isolated fixture resources and bounds before fitting, so unrelated layout
  rendering cannot alter Front, Side, or Bottom framing. Capture slices now
  perform their synchronization through the normal offscreen render lifecycle
  and use the isolated scene's resource base and visibility context throughout,
  restoring the same framing and proportions as uninterrupted manual capture.

- Fixture document editing now prepares changes in private project working storage and
  publishes only derivatives that satisfy the complete fixture-symbol contract, without
  using the fixture library as temporary storage or exposing temporary paths.

- Newly generated fixture symbols now appear immediately in layout legends, previews,
  print output, and PDF workflows without restarting Perastage, while project and GDTF
  changes can no longer reuse stale parsed symbol data.

- Added deterministic cross-platform fixture-symbol regression coverage and
  concise internal generation timing diagnostics to protect existing visual
  output and help diagnose future performance changes, including portable test
  linkage, canonical platform-independent contour geometry, and complete
  diagnostics for skipped and failed work.

- Restored Windows Debug build compatibility for the 2D and 3D viewers by
  ensuring the GLEW header is initialized before platform OpenGL headers.

## Important fixes

- Corrected inverted lighting in the 3D Sketch style by applying its three-tone
  treatment to the neutral output of the proven Standard renderer, preserving
  the same transforms and illumination while restoring smooth surfaces, the
  original stroke-before-fill ink treatment, and a balanced dark, mid, and
  light tonal distribution.

- Replaced the blocking project GDTF consolidation information dialog with a
  non-blocking message in the program console while retaining the diagnostic
  in the persistent application log.

- Fixed MVR fixture replacement so multiple imported source aliases explicitly mapped
  to the same GDTF Share revision and mode share one finalized project fixture. Canonical
  `@Perastage.gdtf` derivatives now require all four stored symbol views before library
  publication, while current projects load those views directly without startup symbol
  generation or a persistent symbol manifest.

- Restored the fixture-symbol archive mutation regression target across CI platforms
  by keeping its cache-invalidation test boundary linked explicitly.

- Preserved imported fixture IDs and unit numbers exactly through project saves and
  reloads, including intentional zero values, instead of retaining temporary
  standards-compliant export substitutions as editable project data.

- Corrected fixture colors across summaries and layout legends by using one
  canonical resolution policy. Legacy projects can now recover a missing
  Perastage fixture color from the official restored MVR color, while colors
  that users intentionally left empty remain empty through refresh, save, and
  reload. Automatic type colors no longer outrank legacy project recovery and
  remain portable in standalone MVR exports.

- Fixed the 3D viewport context menu failing to open in projects without
  fixtures or trusses, on other Data View pages, or when viewport picking is
  temporarily unavailable.

- Improved Windows test reliability for truss Magnet archive and cache
  validation by initializing its native file services explicitly and reporting
  failures without modal dialogs.

- Corrected unconstrained 3D selection dragging after a Magnet preview so both
  pointer projections use the same unsnapped drag anchor.

- Fixed continuous fixture, truss, and scene-object placement drifting away
  from the pointer after zooming, panning, orbiting, resizing, fitting, or
  changing standard views in the 2D and 3D viewers.
- Corrected Bottom and Side view pointer orientation, high-DPI coordinate
  conversion, and Magnet preview refresh so continuous placement remains
  visually anchored through view and framebuffer changes without extra Undo
  entries. Confirming snapped elements now keeps each following provisional
  copy at the raw pointer position without inheriting the previous snap offset,
  including after temporary high-DPI conversion or 3D projection failures.

- Fixed a crash that could occur when opening another project after startup by
  releasing viewer scene caches before the project replaces its scene data.

- Corrected grouped scene transforms so interactive truss moves consistently
  affect their effective groups while fixtures, supports, scene objects, and
  table edits remain exact; compact negative console offsets such as `--1`
  are now accepted, invalid transforms apply atomically without empty undo
  operations, and MVR export validates or safely repairs canonical local
  hierarchy matrices before serialization. Fixture-to-hoist conversion and
  table deletion now preserve valid GroupObject ownership without dangling
  hierarchy references, leave unrelated empty groups intact, and keep actual
  selection distinct from the highlighted interactive movement scope. Valid
  console transforms that are already satisfied no longer add empty Undo
  entries or trigger unnecessary scene refreshes.

- Fixed global shortcuts being suppressed while a read-only combo box has
  focus on macOS.

- Fixed Unicode truss dictionary paths and filenames on Windows while keeping managed asset references portable across platforms.

- Improved PDF output serialization across platforms and locale settings, and
  added deterministic temporary extraction fixtures while retaining unchanged
  compatibility inputs and pinned-library API coverage for final cross-platform
  verification.

- Corrected Rider truss dictionary matching for finish and length variants, and
  ensured GDTF SVG symbols no longer masquerade as renderable 3D geometry when
  an imported truss requires an honest dummy fallback, while resolved truss
  dimensions remain consistent with portable managed dictionary resources.

- Corrected Rider imports so side lighting trusses use their documented default height and LED screens retain accurate dimensions through the shared cube primitive transform.

- Corrected Rider rigging imports so side-fill hoists retain their audio grouping and pipes for lighting bridges, including explicit-length model-free entries, expand consistently across LX positions.

- Standardized filtered Rider previews with compact, stable section spacing across line-ending styles while preventing removed comments from leaving blank sections.
- Stabilized MVR layer and scene-object identities so damaged legacy UUIDs and dependent hierarchy or hoist links recover consistently across project save, reload, and export.
- Preserved Support hoist and Truss metadata through standards-compliant MVR and project roundtrips, including portable auxiliary Truss GDTF resources.
- Hardened MVR metadata recovery against foreign or unsupported legacy payloads, unknown and duplicate identities, invalid numeric and fixture-link values, unsafe auxiliary paths, and missing Truss resources without silent fallback substitution.
- Rejected duplicate and case-colliding MVR archive entries consistently across platforms and surfaced tolerant-recovery diagnostics through normal imports.
- Preserved explicit fixture categories, colors, and valid GDTF mode references across repeated MVR roundtrips while making missing-category inference deterministic.
- Preserved per-fixture visual color overrides, including intentional empty colors, and meaningful fixture ID text in Perastage projects without leaking project-only instance metadata into normal MVR exports or other fixtures of the same type.
- Preserved per-fixture label settings when fixture identities can be safely canonicalized, including recovery from compatible legacy key spellings without guessing invalid or ambiguous identities.
- Improved GDTF compatibility and safety for Unicode filesystem paths and archive entry names, including readable publication diagnostics and deterministic rejection of malformed UTF-8 identities.

## Current limitations

## Technical and packaging changes

- Updated fixture-derivative regression checks for the project-authoritative
  publication policy, including separate project and optional library copies.

- Removed the obsolete whole-scene fixture-symbol startup queue and persistent
  project symbol-manifest save path; finalized project-owned derivatives now provide
  their stored views directly, while explicit regeneration remains available.

- Strengthened cross-platform fixture-symbol persistence verification by
  releasing archive readers before atomic replacement and restoring temporary
  fixture-library configuration deterministically.

- Stabilized the internal GDTF Share security verification by making temporary
  credential fixtures uniquely owned and reliably cleaned up across platforms.

- Updated the internal Viewer2D framebuffer diagnostics verification to track
  cached capture-target failure reasons accurately after cache acquisition.

- Strengthened internal 3DS loader dimension verification with complete bounds
  validation, actionable failure diagnostics, and reliable temporary-fixture
  cleanup.

- Centralized UUID utility build ownership so production and standalone verification targets share one portable compiled implementation.

- Aligned save/load verification with the existing distinction between manually overridden hoist loads and automatically calculated rigging loads.

- Expanded macOS Debug continuous integration to build and exercise the complete registered test suite, matching Linux and Windows coverage while preserving diagnostic artifacts.

- Strengthened internal MVR 1.6 compliance coverage by separating primitive
  geometry contracts from malformed SceneObject identity-recovery controls.

- Standardized embedded layout-image resource names in project archives as
  portable UTF-8 paths with forward slashes on every platform, and strengthened
  layout-package validation against unsafe names hidden by ZIP path
  normalization.

- Improved maintainer issue intake and triage with clearer diagnostic guidance and revision-aware, failure-resilient automation limited to reports explicitly awaiting feedback.

- Aligned CI policy checks with the validated Windows Git Bash initial-cache data flow and strengthened cross-platform filesystem path boundary coverage.

- Added native object-cache acceleration with GitHub-scoped CI isolation, resource-aware Windows embedded-debug validation, and measurable compiler-cache diagnostics without changing installer, dependency-cache, or test behavior.

- Added trusted cross-platform Debug cache warming after changes reach `main`, improving compiler-cache consistency in later pull requests while preserving isolated PR writes, reliable policy checks, and the full PR test suite.

- Improved dependency-cache warming reliability with complete Linux and macOS build prerequisites, serialized package publication, and credential-safe failure diagnostics.

- Fixed GitHub Packages dependency-cache initialization so clean GitHub Actions runners receive an isolated NuGet configuration before the cache source is added.

- Improved GitHub Packages dependency-cache setup with reliable structural validation and safe, redacted failure diagnostics across all supported build platforms.

- Added a secure, platform-compatible persistent dependency cache with main-only publishing for trusted GitHub Actions builds while retaining fast local workflow caches and read-only behavior for CI and installers.

- Improved GitHub Actions vcpkg caching so dependency builds are saved immediately after successful installation and can be reused across compatible CI and installer workflows.

## Internal changes

- Completed the fixture-symbol background-generation architecture by removing the
  obsolete project manifest and generation identity, unifying stored-symbol
  availability checks, and replacing archive hashing in normal SVG cache lookups
  with bounded file revision checks plus explicit lifecycle invalidation.

- Kept isolated fixture-capture visibility policy explicit at the renderer
  boundary so focused bounds tests and Windows builds remain independently
  linkable without depending on the runtime scene manager.

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
