# Perastage v1.6.0 Release Notes

Changes since **v1.5.0**.

## Highlights

- Create from text now stops before scene creation when a rider contains unknown
  fixture types, presents a resolution table with cached GDTF Share suggestions,
  explicit mode selection and Generic fallback, and remembers confirmed GDTF
  mappings for later riders.

- Automatic DMX patching now keeps logical lighting Positions and their
  physically separate bridge components together whenever universe capacity
  allows, including deterministic serpentine patching for parallel side
  structures without relying on position names.

- Export MVR now suggests the current project name as the destination file
  name, matching the Save Project workflow while still allowing it to be
  changed before export.

- Textures, images, and external binary buffers used by 3DS and glTF geometry
  now survive MVR export and re-import, while dependencies of deleted models
  continue to be removed and filename collisions fail safely instead of
  producing broken model references.

- MVR export now presents concise, grouped fidelity warnings instead of raw
  technical diagnostics, while harmless UUID normalization stays in the log and
  export failures provide a focused, expandable result. The collapsed result
  remains compact and uses reliably rendered summary markers on every platform.
  Missing fixture profiles, unresolved positions, and failures to preserve
  edited profile properties or required object geometry are now reported
  explicitly instead of being silently omitted, with Linux export regression
  coverage preserving the existing fixture-address and legacy truss migration
  contracts.

- Project reopening is faster for typical projects by loading packaged scene
  and configuration data directly, while retaining a safe large-project
  fallback and all existing MVR resource compatibility.
- Help menu commands now consistently open their intended help, documentation,
  log, diagnostic, update, and About actions instead of unrelated Tools
  workflows.
- Deferred fixture-symbol preparation now reuses unchanged GDTF inspection
  results instead of repeatedly scanning the same profile.

## Fixes

- Fixture placement in the 3D viewer now acquires visible truss attachment
  paths within a small screen-space aperture regardless of camera-depth
  separation, while still committing fixtures onto the actual 3D path.

- Create-from-text fixture-resolution checks now use the repository's portable
  test launchers consistently across Debug CI platforms.

- Resolve fixture types now builds reliably with MSVC when displaying an empty
  fixture-mode selection.

- Create from text now keeps Generic fallback immediately available, excludes
  screens and video-control equipment from fixture resolution, shows cached
  suggestions without delaying the dialog, and rejects empty GDTF Share
  catalogs without replacing the last known good cache.
- Fixture resolution now automatically uses the shared MVR catalog matcher,
  supports corrected fixture names and per-type creation choices, and searches
  combined manufacturer and model identities such as GLP JDC1.
- Downloaded GDTF Share files now retain external-source identity when mapped
  from rider text, and recoverable download, mode, or dictionary failures fall
  back only the affected fixture type instead of cancelling scene creation.
- Resolve fixture types now shows real catalog and per-fixture matching progress,
  retains a final match summary, and uses the same semantic status colors as the
  MVR GDTF download workflow.
- Resolve fixture types now keeps stable table rows during background matching
  and owns its cancellable catalog worker, preventing Debug assertions when
  rows update or the dialog closes while matching is active.
- Resolve fixture types now uses a fixed eight-column, analysis-backed table
  model, preventing Windows Debug assertions caused by an empty model schema
  while preserving live matching updates, sorting, editing, and status colors.
- Resolve fixture types now reports the mappings actually shown in the final
  table and uses portable separators, avoiding misleading match totals and
  incorrectly encoded summary characters on Windows.
- New installations can now acquire the shared GDTF catalog directly from the
  fixture resolver using the established Download GDTF sign-in and cache flow.
  Manual assignments remain visually distinct, and downloaded profiles use
  readable catalog-based filenames instead of numeric revision identifiers.
- Catalog sign-in, download, parsing, and automatic matching progress now stay
  inside Resolve fixture types instead of opening additional progress windows,
  with stable completion, cancellation, and offline-fallback status messages.
- Automatic rider catalog matching now skips fixture types already resolved by
  the active dictionary or an explicit choice, reducing unnecessary matching
  work and making progress totals reflect only unresolved rows.
- Shared colored data-view stores now validate item ownership and model-column
  bounds before value access or sorting, and resolver row updates publish one
  final attribute notification instead of exposing partial Status changes.

- Rider-import test executables now link reliably when using the shared fixture
  dictionary alias normalization required by Create from text analysis.

- Download GDTF now requests sign-in before loading an uncached catalog and no
  longer opens an unusable empty search window when credentials are missing,
  rejected, or the online catalog cannot be loaded. Cached catalogs remain
  available for offline browsing, and cancelling sign-in ends the operation
  cleanly.

- Saved Layouts now retain bounded 2D-view and legend previews even when their
  vector replay data is too large to cache, reducing repeated rendering work
  when reopening unchanged projects.

- Warm Layout previews now validate against the exact scene and packaged image
  data stored in the project, avoiding false cache invalidation after normal
  MVR identity normalization.

- Warm Layout startup now restores legends across temporary extraction
  workspaces, avoids creating offscreen 2D rendering infrastructure when cached
  previews are sufficient, and gives active Layout rendering priority over
  deferred fixture-symbol inspection.
- Persistent Legend previews are now validated only after their current
  semantic content is prepared, so an unchanged saved Layout restores its
  Legend immediately instead of rebuilding symbols after every reopen.

- Fixture profiles that contain neither stored symbols nor usable GDTF
  geometry now use Perastage's deterministic runtime fallback directly. This
  avoids repeated symbol preparation delays when opening projects while
  preserving reliable automatic symbol generation for renderable fixture
  profiles across supported build environments, including dimension-defined
  models that intentionally omit a file and primitive type while retaining
  fallback behavior for explicitly undefined models.

## Fixes

- MVR-xchange TCP Mode now interoperates with applications such as grandMA3
  that send the standards-defined source-station UUID array when requesting a
  published revision, while retaining compatibility with the contradictory
  single-string request example in MVR 1.6. Outgoing requests now identify the
  remote station that owns the advertised revision, resolving request UUID
  mismatch rejections from applications such as DMXRouter. DNS-SD advertising
  now keeps group and station-instance identities distinct, uses a
  collision-resistant station host name, and strictly associates each peer's
  identity with its own SRV endpoint. Independent mDNS responders remain
  isolated, and exact local listener endpoints are rejected to prevent
  accidental self-connections. Joined peers may now retain their TCP connection
  for later requests, while applications that reconnect for each operation
  remain supported.

- Fixtures of the same type now receive one consistent type color when
  text-to-scene generation cannot find their definition in the GDTF library.

- Create from text now recognizes common Spanish and English front, middle,
  rear, side, floor, and screen headings without mistaking words inside
  equipment descriptions for section headers. Effects and lighting-control
  lists no longer leak into fixture imports, while VIDEO screen subsections,
  canonical filter output, and existing rigging commands remain compatible.
  Apply filter and the initial Create filtering stage also remain responsive
  when processing long or heavily formatted rider text.

- Fixture-symbol rendering now notices GDTF files replaced at the same path,
  rejects stale runtime and disk geometry caches, keeps the authoritative
  dimensions of file-backed GDTF models isolated when several definitions
  share one mesh, generates fixture-type symbols at a canonical origin without
  inheriting instance translation, rotation, scale, shear, or parent placement,
  and restores the exact offscreen 2D viewer state after Layout capture work.
  Resource synchronization, visibility, rendering, and bounds calculation now
  share one platform-neutral reference-key contract. Fixture capture now uses
  the renderer's loaded GDTF bounds for its established framing, while physical
  calibration uses the same metre/millimetre transform contract as rendering.
  Together these changes prevent renderable fixtures from being framed with the
  small unresolved-resource fallback solely because their path uses a different
  slash or relative-path spelling.

- Layout 2D View elements now refresh after any visual scene change, including
  adding, moving, editing, or removing scene elements as well as opening or
  merging MVR files, so saved layouts cannot continue showing stale content.
  Preview capture no longer feeds back into scene-change invalidation, avoiding
  repeated Layout render queues when changing views, and regenerated previews
  now discard saved rasters and read the latest scene contents after elements
  are removed from a viewport or table.

- Restored the established fixture-symbol capture and processing pipeline for
  consistent manual generation of compound-fixture symbols.

- Automatic GDTF Share matching during MVR import now rejects unrelated
  fixtures that share only descriptive words, technical annotations, or DMX
  characteristics, while preserving strong model-family matches and falling
  back safely when catalog identity is weak or ambiguous. Automatic import and
  the Search GDTF dialog now use the same catalog records, ensuring available
  exact fixture models are consistently found. MVR object labels and generic
  descriptive terms can no longer override the resolved fixture-model identity.

- Restored macOS 15 Apple Silicon packaging compatibility with a dedicated
  worker fallback while preserving the established fixture-symbol processing
  behavior on Windows, Linux, and supported macOS toolchains.

## New features and workflow improvements

- Both 2D and 3D viewports now support Middle Mouse drag panning. The 3D viewer
  also provides independent horizontal and vertical orbit-inversion preferences
  that remain personal across project loads, while preserving existing
  vertical-inversion settings and default navigation. Middle Mouse panning also
  remains available while positioning provisional elements in the 2D view.

- Cut, Copy, and Paste now use their matching Lucide icons in the Edit toolbar,
  making the scene clipboard controls easier to identify at a glance.

- Added a unified fixture distribution dialog on `Alt+D`. Alongside the
  existing uniform full-truss and two-point systems, fixtures can now use an
  exact configurable center or edge gap, distributed outside-in between two
  limits or from a start point in a chosen direction. Distributions remain
  undoable and report non-blocking feedback when the requested layout does not
  fit. Edge-to-edge placement uses the loaded fixture dimensions consistently
  in Debug and Release builds.

- Added two truss-line fixture distribution tools. Selected fixtures on one
  straight truss can be spaced evenly across its full length with equal end
  margins, or between two pointer-selected points in the 2D viewport. Both
  workflows preserve selection order, support Undo and Redo, provide
  non-blocking validation feedback, and allow point selection to be cancelled
  with `Esc`.

- Added a project-scoped scene clipboard foundation for safely cloning and
  removing mixed fixture, truss, hoist, and scene-object selections while
  preserving MVR identities, hierarchy relationships, and fixture label data.
  Cut, Copy, and Paste are available from the Edit menu, toolbar, and the
  standard `Ctrl+X`, `Ctrl+C`, and `Ctrl+V` shortcuts. Single-item Paste now
  enters repeated cursor-driven placement in either scene viewport, including
  hoists, while preserving the original clipboard data for every placement.
  Multi-item Paste now enters rigid pointer-driven batch placement in either
  scene viewport with raw cursor tracking, exact leaf transforms, and no Magnet snapping.
  Repeated single-item Paste now records one Undo step per confirmed click,
  cancels provisional copies without rewinding history, and removes transient
  copies before save, export, project replacement, or application shutdown.
  Confirming a Magnet-snapped clipboard item now finalizes its durable grouping
  before the per-click history snapshot is committed.

- Fixtures can now snap continuously along geometry-derived main chords on
  straight square, triangular, and ladder trusses. Fixture guidance shows these
  mounting paths instead of structural connector points, while existing GDTF
  Magnet behavior remains dedicated to truss-to-truss connections. When usable
  chord geometry is unavailable, Perastage retains its conservative bounds-based
  fallback.

- Fixture-to-truss attachment now prefers the structural model explicitly
  referenced by GDTF Structure geometry, including its complete placement
  hierarchy, before considering generic GDTF or MVR geometry. Viewer guidance
  draws the exact continuous attachment paths used for snapping.

- Added an enabled-by-default **Magnet visual feedback** preference that shows
  every compatible anchor as a vivid red point or direction line throughout
  movement and insertion in the 2D and 3D viewers.

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

- Magnet guidance now hides truss connectors that are already joined. Fixture
  attachment paths remain consistently vivid red in Sketch mode and no longer
  produce unbounded lines when a path crosses behind the 3D camera.

- Added **Selection & Movement** preferences for choosing, independently for
  fixtures, trusses, supports/hoists, and scene objects, whether mouse,
  command-bar, and Magnet transforms move an exact grouped object or its
  highest containing group. Table edits remain exact and project/MVR hierarchy
  data is unchanged by the preference.

## Compatibility, stability, and performance

- Improved startup responsiveness and visual stability by restoring the saved
  project view directly, creating heavyweight 2D and 3D viewports only when
  required, populating project tables once, and transferring persistent layout
  cache data during the primary project archive read. Startup diagnostics now
  report measured restore details at a formal interactive-ready boundary in
  both diagnostic logs and the internal CMD panel, while automatic
  fixture-symbol preparation begins afterward. Saved 3D and 2D projects now
  reliably create their required lazy viewport before perspective restoration,
  and missing custom toolbar resources produce actionable diagnostics instead
  of an unexplained fallback.

- The startup splash now remains in place until the fully restored project
  window is ready to publish, eliminating the brief display of temporary pane
  arrangements. The standard 3D, 2D, and Layout commands now restore complete,
  history-independent panel sets, and all five main toolbars—including the
  scene Tools toolbar—remain visible with disabled tools retaining their slots.

- Stabilized MVR-xchange TCP Mode interoperability and LAN-input handling. Deterministic per-station announcements avoid JOIN/COMMIT duplication and reciprocal JOINs for incoming members, explicit leave membership survives passive rediscovery, typed failures and canonical sender fields improve compatibility, and persistent multicast discovery now uses scoped DNS-SD resolution, TXT merging, TTL lifecycle handling, bounded multipart transfers, and steady-state query backoff.

- Improved MVR-xchange interoperability with consoles that follow the official
  adjusted JOIN response example without a `Provider` field, while retaining
  canonical output and strict station identity and inventory validation.

- Made the MVR-xchange DNS goodbye regression portable across Windows, Linux,
  and macOS debug test configurations.

- Fixed cross-platform station identity promotion so provisional discovery
  records merge safely when an authoritative station UUID becomes available.

- Added a dedicated remote-station view and one-click log copying to the
  MVR-xchange dialog, reduced repeated discovery diagnostics, and improved
  incoming compatibility with consoles that advertise the standard new-member
  `0.0` version before sharing their MVR revisions.

- Compacted MVR-xchange station settings and gave the independently scrollable
  station and advertised-file lists more usable space without enlarging the
  dialog.


- Unified project and standalone MVR serialization around one canonical MVR 1.6 contract. Both interactive commands now prepare derived scene data consistently; portable Perastage scene-fidelity metadata follows fixtures safely through standalone imports and merges; valid third-party root UserData is preserved without duplication; malformed provider blocks are diagnosed and excluded; and read-only snapshots no longer repair or dirty the live scene. File > Export MVR now honors the explicit, MVR 1.6-compliant Direct Geometry3D truss compatibility preference without affecting project persistence or canonical snapshots.

- Improved Windows CI reliability by adding a pinned direct ripgrep fallback
  when the hosted runner does not provide the tool and Chocolatey installation
  is unavailable or temporarily fails. This keeps the cross-platform Debug test
  gate independent of transient runner package-manager state.

- Stabilized the cross-platform test suite by distinguishing optional MVR truss
  GDTF references from required Geometry3D data, accounting for the documented
  Form XObject limitation in legacy PoDoFo, and cleanly skipping native-widget
  focus checks when a Linux build has no graphical display. GDTF-authoritative
  truss exports now also generate the required GDTF when their source was
  imported as MVR geometry. Binary MVR compliance fixtures now use explicit
  fixed-width integer types so they build consistently with AppleClang and
  libc++ on macOS. Geometry cache timestamps also use a portable signed
  nanosecond representation, avoiding ambiguous libc++ conversions on macOS.

- Fixed a Debug-build assertion that could stop Perastage when movement began
  with Magnet references enabled. Runtime truss chord analysis now reads only
  CPU geometry, avoids re-entering wxWidgets image-handler initialization, and
  uses viewer-owned caches for safe repeated snapping and overlay updates. The
  resolver also continues to lower-priority geometry when a preferred resource
  has no reliable chords and reports bounds fallback consistently for poor data.

- Trusses loaded from 3DS or GLB geometry now use their measured local bounds
  instead of fixed nominal dimensions. This repairs legacy dimension metadata,
  generated GDTF sizing, and Magnet endpoints while preserving explicit GDTF
  dimensions and connector definitions. The shared geometry parsers are now
  independent of viewer console state, including in Windows test builds.
  Geometry-backed MVR trusses using either direct Geometry3D or Symbol/Symdef
  resources now recover the same coherent dimensions during project loading.

- Continuous fixture, truss, and scene-object insertion in the 3D viewer now
  presents each placement update while the pointer is moving and returns the
  preview directly beneath the pointer when axis-constrained movement is
  disabled, instead of waiting for pointer activity to stop or retaining the
  offset accumulated while the constraint was active. With the constraint
  enabled, moving the pointer away from the active projected axis now switches
  the preview to the newly intended axis, realigns it beneath the pointer, and
  starts the new constrained movement without carrying over the previous
  axis offset. A short travel along the new axis prevents repeated realignment
  from making constrained movement feel unlocked. Axes that point almost
  directly into the camera are ignored to prevent extreme, off-screen placement
  jumps from small pointer movements, and moved previews return when brought
  back into view after leaving the rendered scene area. When multiple world axes
  overlap in screen direction, placement now prefers the axis with the clearest
  screen projection instead of selecting an arbitrary, foreshortened axis.

- Corrected generated fixture-symbol framing and physical bounds to use the
  selected GDTF mode and every rendered part, improving scale and placement
  for asymmetric and mode-dependent fixtures.

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

- Fixture symbol capture now uses one private, non-yielding compatibility scope
  for warm-up and all four orthographic views, then restores the project exactly
  once before returning to the interface. This prevents distorted non-top symbols and
  ensures layout 2D views and printed layouts retain fallback or stored-symbol
  geometry while preparation progresses. Automatic and manual generation now share
  the same continuous renderer semantics, so unrelated project contents cannot alter
  Front, Side, or Bottom framing or component placement.

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

- Fixed truss-line distribution validation for scene coordinates stored in
  millimeters and for straight bridges assembled from multiple connected truss
  sections. Distribution and Magnet guidance now use the same resolved hang
  geometry, preventing different truss chords from being mixed. Two-point
  distribution also shows fixed and moving green endpoint markers directly on
  the active red hang line while hiding unrelated Magnet paths and selection
  overlays throughout the endpoint-picking interaction. Starting the tool now
  works in either the active 2D or 3D viewport, while `Esc` or right-click
  cancels the interaction and clears its status prompt. Distribution commands
  now require viewport focus and never open, close, or switch viewer panes.
  Endpoint markers follow the pointer's dominant screen axis for intuitive
  horizontal and vertical placement, and both distribution modes retain the
  original fixture selection after deferred table refreshes complete. Live
  cursor sampling and perspective-correct projection keep endpoint markers
  aligned during viewport redraws. Endpoint clicks are now fully consumed by
  the active tool so the first click cannot clear the fixture selection.

- Restored clear hover, group, and selection highlighting in the 3D Viewer
  Sketch and Wireframe styles. Sketch highlights now retain the same vivid
  colors, light-and-shadow shading, and black edge definition as the other
  filled styles by using the Standard highlight material after Sketch
  composition with a depth-safe overlay, while Wireframe feedback remains
  visible over its lines.

- Project opening now completes its progress dialog explicitly before closing
  it, preventing an intermittent stall at "Finalizing project load..." during
  application startup.

- Smoothed diagonal and curved geometry edges in the 3D Viewer Sketch style by
  preserving the viewer's configured antialiasing through Sketch composition,
  without changing the appearance of the other render styles.

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

## Fixes

- Fixed Windows compilation of Layout startup profiling and lazy offscreen
  rendering by making their implementation dependencies explicit.

- Incremental builds now refresh the source revision shown in diagnostics when
  the active Git commit changes.

- Restored successful Windows Debug linking for project, dictionary, and rider
  test executables that use lightweight MVR test implementations.

- Corrected automatic GDTF Share selection during MVR import so official mode
  and footprint metadata, fixture-type identity, manufacturer, and model-number
  evidence choose the most compatible catalog fixture instead of relying on an
  MVR object's display name or catalog popularity.

## Internal changes

- Improved MVR-xchange TCP lifecycle diagnostics for cross-platform test runs.

- Made the fixture-symbol processing ownership check portable across the Bash
  versions and filesystem path conventions used by Debug CI runners.

- Consolidated GDTF fixture identity reads through the existing loader cache and
  corrected test linkage for the shared UUID implementation.

- Restored reliable standalone MVR importer test builds by keeping their GDTF
  metadata and catalog dependencies aligned with the production importer.

- Completed the fixture distribution regression target's Magnet projection
  dependencies for reliable Windows linking.

- Fixed the Windows scene clipboard GUI build by making its configuration
  service dependency explicit.

- Fixed standalone scene clipboard test linkage so Windows builds reuse the
  shared UUID, diagnostics, logging, and application-path test dependencies
  without compiling duplicate implementations.

- Reused parsed GDTF truss attachment metadata across repeated snapping and
  overlay updates, avoiding redundant archive reads when only an instance moves.

- Completed the fixture-symbol background-generation architecture by removing the
  obsolete project manifest and generation identity, unifying stored-symbol
  availability checks, and replacing archive hashing in normal SVG cache lookups
  with bounded file revision checks plus explicit lifecycle invalidation.

- Kept isolated fixture-capture visibility policy explicit at the renderer
  boundary so focused bounds tests and Windows builds remain independently
  linkable without depending on the runtime scene manager.

- Improved cross-platform fixture-symbol regression coverage by using the
  configured test interpreter and releasing inspected derivative files before
  transactional replacement on Windows.

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
