# Architecture and repository conventions

This document is the authoritative source for Perastage's architectural
ownership, module responsibilities, and accepted dependency directions. For
the canonical human-readable map of repository paths and root-file roles, see
[Repository Layout](repository_layout.md).

## Top-level layout

<!-- repository-source-module-responsibilities: app=application lifecycle and startup composition; cli=headless command-line entry and grammar; core=shared business logic and services; gui=wxWidgets UI and workflows; models=scene data structures; mvr=MVR interchange; viewer2d=2D rendering and export; viewer3d=3D rendering and loading; viewer_common=shared viewer utilities -->

This stable marker is checked against the canonical source-module inventory in
`repository_structure_baseline.json`; the descriptions below define each
module's architectural responsibility.

- `app/`: wxWidgets application lifecycle and startup composition.
- `cli/`: headless command-line entry point and stable technical grammar.
- `core/`: shared business logic and services.
- `gui/`: wxWidgets UI and main window workflows.
- `viewer2d/`: 2D renderer and PDF/export helpers.
- `viewer3d/`: 3D renderer, loaders and render passes.
- `models/`: core scene data structures.
- `mvr/`: MVR import/export modules.
- `viewer_common/`: utilities shared by the 2D and 3D viewers.
- `third_party/`: vendored third-party single-header dependencies (for example `json.hpp`, `stb_easy_font.h`).
- `library/`: bundled runtime content (fixtures, trusses, `scene_objects`, examples).
- `tools/mcp/`: standalone Rust stdio MCP adapter over `perastage-cli`; it is
  outside the native application module graph.

## Third-party convention

- Keep vendored code under `third_party/` only.
- Do not place third-party libraries inside feature modules (`core/`, `gui/`, `viewer*`, etc.).
- Prefer package-managed dependencies for compiled libraries; use `third_party/` for vendored sources/headers only.

## CMake convention

- Root `CMakeLists.txt` owns project options, principal target creation, shared target configuration, and module orchestration. Focused modules own dependency discovery (`PerastageDependencies.cmake`), localization (`PerastageLocalization.cmake`), runtime staging (`PerastageRuntimeStaging.cmake`), installation (`PerastageInstall.cmake`), and packaging (`PerastagePackaging.cmake`).
- `cmake/platform/PerastagePlatform.cmake` dispatches target-level platform configuration to separate Windows, macOS, and Linux owners after the application target exists. Linux desktop, MIME, and icon integration remains installation configuration rather than target configuration.
- Every top-level application source module registers its production sources explicitly in its local `CMakeLists.txt`. Most sources use `target_sources(${PROJECT_NAME} ...)`; focused reusable targets own their sources directly and are linked by the application.
- `docs/developer/repository_structure_baseline.json` is the authoritative machine-readable contract for source-module classification and CMake registration.
- Avoid recursive or wildcard project-source discovery; list files explicitly.
- Keep include directories close to the module that owns them.
- Root C/C++ sources are restricted to explicitly approved architectural entry
  points. `main.cpp` is the sole current exception; approving another requires
  aligned baseline root-source and root-role contracts plus architecture and
  repository-layout documentation.

Introducing a top-level production source module requires one coordinated
change: baseline classification and guard-set alignment, a local
`CMakeLists.txt` with explicit `target_sources(...)` ownership, root
`add_subdirectory(...)` registration, and descriptions in both this document
and the repository layout. Dependency directions remain separately reviewed;
the guard never adds an accepted edge for a new module automatically.

### Application include-directory ownership

Feature include directories are declared by their owning module's local
`CMakeLists.txt`, using module-relative paths. Root application includes contain
only shared vendored headers (`third_party`), dependency-provided compatibility
paths (`${_wx_includes}` and `${NANOVG_INCLUDE_DIR}`), and the retained
`resources` compatibility exposure. Do not expose the repository root as a
catch-all include directory or duplicate feature include declarations in root.

`resources` contains platform resources and runtime assets, rather than
application headers. Removing its compatibility include requires native
Windows, Linux, and macOS build evidence for the same revision. Dependency
include modernization is a separate reviewed change.

Test executables declare their own focused include requirements in
`tests/CMakeLists.txt`; these do not justify application-target paths.
[`check_include_directory_ownership.py`](../../tests/check_include_directory_ownership.py)
owns the exact include inventory and validates root/module declarations.

All GUI feature modules still contribute to the same `${PROJECT_NAME}` target.
Moving declarations to module CMake files records ownership and reduces root
coupling, but does not create source-local compiler isolation or a new target
boundary. Stronger compile-time isolation would require a future target-level
architecture change.

### Internal module dependency directions

The following contract records the current production source-level includes.
It is not a target-level link graph: the GUI application modules contribute
to the application target, while `cli` owns separate command-line targets.
Same-module includes and test sources are excluded; exact include evidence
belongs to the guard inventory, not this contract.

| Consumer | Accepted providers | Architectural rationale |
|---|---|---|
| `app` | `core`, `gui`, `viewer3d` | Application composition coordinates lifecycle services, windows, and the existing GDTF cache teardown API. |
| `cli` | `core` | The headless command-line shell consumes only the canonical Core product name and public Inspection APIs; semantic readers remain transitive implementation details. |
| `core` | `models`, `mvr`, `viewer2d`, `viewer3d` | Application services coordinate scene data, interchange, and existing symbol/geometry implementations. |
| `models` | None | Scene data does not include another production module. |
| `mvr` | `core`, `gui`, `models`, `viewer2d`, `viewer3d` | Interchange uses shared services and scene data plus existing import presentation, label, and geometry facilities. |
| `gui` | `core`, `models`, `mvr`, `viewer2d`, `viewer3d`, `viewer_common` | UI workflows orchestrate application services, interchange, both viewers, and shared GL utilities. |
| `viewer_common` | `core` | Shared viewer utilities use central preferences and diagnostics. |
| `viewer2d` | `core`, `gui`, `models`, `viewer3d`, `viewer_common` | 2D rendering consumes scene/services, shared GL support, existing 3D types, and UI command identifiers. |
| `viewer3d` | `core`, `gui`, `models`, `viewer2d`, `viewer_common` | 3D rendering consumes scene/services, shared GL support, 2D render interfaces, and existing UI status facilities. |

The current graph contains intentional-for-current-architecture cycles between
`core` and each of `mvr`, `viewer2d`, and `viewer3d`; between `gui` and each of
`mvr`, `viewer2d`, and `viewer3d`; and between `viewer2d` and `viewer3d`.
In particular, renderer-to-GUI includes, `mvr -> gui`, and Core's viewer
includes are visible technical-debt directions rather than a proposed layering
model. They are accepted compatibility directions; removing them requires a
separate behavior-preserving architecture change.

`tests/check_module_dependency_directions.py` resolves quoted includes through
source-relative paths and the application's ordered include roots, then rejects
ambiguous project headers and directions outside this reviewed set. A legitimate
new direction requires architectural review and a coordinated update to this
section and the check's explicit `ACCEPTED_DIRECTIONS`; the check never blesses
new edges automatically.

## Application bootstrap ownership

`app/` owns application composition. `app/perastage_app.h` declares
`MyApp : wxApp` and its lifetime state; `app/perastage_app.cpp` implements
lifecycle methods and private bootstrap helpers. Root `main.cpp` only includes
the App declaration and invokes `wxIMPLEMENT_APP(MyApp)`.

App owns launch-CWD capture, startup argument/path selection and external-open
queuing, CLI/platform open routing, two-stage deferred startup resolution,
localization and fallback warnings, diagnostics and crash handling, splash
sequencing, `MainWindow` construction, metrics, platform hooks, and ordered
shutdown. Core retains reusable services, GUI retains visible startup
publication, and Viewer3D retains cache implementation. App's Viewer3D
dependency is composition debt for the GDTF cache shutdown API. No lower-level
module may include App headers or depend upward on lifecycle policy.

`app/CMakeLists.txt` explicitly contributes the declaration and implementation
to `${PROJECT_NAME}` and exposes only its local include directory. Root CMake
registers App after lower-level modules and owns only `main.cpp` and generated
build information in `add_executable`. The repository baseline,
[`bootstrap ownership guard`](../../tests/check_application_bootstrap_ownership.py),
dependency guard, startup-publication guard, and localization scanner enforce
these boundaries. See [Startup Architecture](startup_architecture.md) for
startup sequencing and publication rules. Internal startup helpers may be
split in focused changes while preserving this ownership and lifecycle.

The GUI MVR download queue owns presentation and terminal acknowledgement;
its synchronous workflow limitations and future CLI sharing points are described
in [MVR GDTF download queue lifecycle](technical-notes/mvr_gdtf_download_queue.md).

## GDTF Share catalog browser

The read-only GDTF Share browser keeps catalog parsing/domain in
`mvr/gdtf_catalog_parser`, cache policy and the GUI-independent search/presentation
model in Core, and widgets/background orchestration in GUI. MainWindow delegates
through the existing download workflow. See
[GDTF Share catalog browser](gdtf_catalog_browser.md) for metadata, TTL, and
worker/session ownership contracts.

## Command contract

Core owns the GUI-independent semantic command boundary under `core/command/`.
`perastage_command_core` owns requests, results, execution context, and the
transactional scene/selection mutation boundary; the focused
`perastage_command_serialization` target owns versioned JSON output. Frontends
remain responsible for presentation refresh. The separate
`perastage_command_text_parser` adapter target owns reusable human Console
grammar and depends toward the semantic diagnostic contract; Command Core does
not depend on human syntax. Semantic transform and typed UUID selection
algorithms are owned by the focused Command Core transform and selection
targets, including compatible selection-only `clear` execution. Exact targeted
batch transforms reuse the selection transforms' component helpers and existing
hierarchy synchronization, validating and previewing ordered rows before one
transaction publishes. The selection
text adapter resolves numeric Console IDs against an explicitly supplied scene.
The embedded Console acquires project context, presents results, and refreshes
GUI state as an adapter frontend; it invokes the focused GUI-independent text
processor once per entered line and does not own parsing, variant dispatch,
semantic adaptation, execution ordering, or stop decisions. The processor
executes parsed commands against one explicit `ExecutionContext`, so selection
from an earlier operation is immediately available to a later transform. It
returns ordered semantic `Result` records, per-step selection snapshots, parser
diagnostics, and an aggregate `MutationSummary`. The Console maps those neutral
facts to the target-specific table/viewer presentation; Core has
no knowledge of widgets, active GUI state, or refresh mechanics.
The focused `perastage_command_scene_tools` target adapts grouping and
explicit destructive conversion operations downward to existing Core scene
services. The shared GUI `GuiProjectMutationHost` is the application-side Undo
publisher used by Console and current main-window command consumers; refresh
and presentation remain frontend-owned. Auto patching and fixture distribution
remain with their existing owners until their implicit scope/geometry inputs
have neutral contracts. The focused `core/capability/` boundary describes the
real Command and Query surfaces without dispatching them. It consumes neutral
operation IDs and request value types without linking Command execution or
mutation infrastructure; separate serialization provides versioned discovery
JSON. `core/local_ipc/` owns a scene-agnostic, loopback-only transport and
versioned request envelope. The development CLI may depend directly on this
neutral transport as its local client, but not on the adjacent `core/live/`
executor. That server-side adapter composes the envelope with existing Command
text processing, typed semantic selection updates and batch transforms, and read-only Query Core
discovery against an explicit execution context. Optional structured arguments
extend the versioned transport contract without adding scene logic to it.
Application composition owns active-project access,
cancellable GUI-thread dispatch, and routing mutations through the existing
refresh boundary. Selection-only publication updates all four tables and viewer
highlights without refreshing or mutating scene content. The transport remains
independent from MVR-xchange and scene logic. See
[`command_api.md`](command_api.md) for the current contract.

`core/osc/` owns the separate bounded OSC 1.0 message parser, IPv4-loopback UDP
transport, and a focused semantic Command adapter. Transport and parsing do not
depend on scenes, GUI code, Console grammar, local IPC, or MCP. Application
composition dispatches parsed messages to the GUI thread and uses the same
active-project execution context, mutation host, transform policy, selection
publication, and refresh boundary as other live commands.

The standalone `tools/mcp/` adapter depends in the opposite direction only at
the process contract: it invokes `perastage-cli` with explicit argv and reads
structured JSON and exit codes. It has no native link or source dependency on
Core, GUI, models, MVR/GDTF parsing, OSC, or local IPC. Its tools map typed
inspection, capability/scene discovery, typed UUID selection, selection-clear,
single-axis transforms, and targeted batch transform requests to stable CLI forms.
The batch tool flattens structured targets/components to aligned typed lists;
mutation and transform algorithms remain owned by Command Core. Stdio is its sole MCP
transport.

## Query contract

Core owns the GUI-independent, read-only scene query boundary under
`core/query/`. `perastage_query_core` accepts only an explicit `const MvrScene&`,
the semantic `ObjectSelection`, and an optional caller-supplied fixture
footprint resolver. It returns deliberate value descriptors and never exposes
scene containers or pointers. Query operations cannot create Undo entries,
dirty a project, refresh UI, download resources, or resolve GUI preferences;
Commands remain the sole shared mutation boundary. The leaf
`scene_object_identity.h` contract gives Command and Query code the same typed
UUID semantics without making Query depend on command execution.
The neutral `fixture_patch_address.h` leaf owns strict parsing of stored DMX
addresses for Query Core, AutoPatch, and MVR merge warning analysis; it has no
scene mutation or presentation responsibility.

The stable query identifiers are `scene.summary`, `scene.selection.get`,
`scene.objects.list`, `scene.object.get`, `scene.layers.list`,
`scene.groups.list`, and `scene.patch.status`. Results use explicit stable
sorting rules. Patch analysis owns strict parsing of the stored
`universe.channel` form and interval overlap detection. Footprints are
dependency-injected so Query Core does not depend on the viewer GDTF loader;
invalid addresses, unresolved footprints, and ranges crossing channel 512 are
reported explicitly rather than assigned guessed footprints. See
[`query_api.md`](query_api.md) for result semantics.

Inspection diagnostics remain separate. Existing Inspection Core reports are
about the exact file or immutable byte source inspected and are not attached to
an active scene that may since have changed. Query Core exposes no
validation report without a service that guarantees current active-project
provenance; Query Core does not duplicate the MVR/GDTF validators.

## Tool and inspection contract

Core owns the neutral request, structured result, and diagnostic types under
`core/inspection/`. The contract is GUI-independent and read-only: it identifies
a filesystem input and preserves ordered diagnostics with stable technical
identifiers, severity, domain, classification, and optional source metadata.
The headless CLI and wxWidgets Inspector are independent production frontends
that consume these structured results directly. Serialization and presentation
are outside this semantic contract: the CLI owns its reports, while
`gui/inspection/` owns only native presentation and layout integration.
The adjacent `gui/inspector_models.*` toolkit-independent package-tree,
scene-tree, issue-grouping, and text-search models project existing Inspection
Core facts without parsing MVR or GDTF data. The current-project source adapter
in `gui/inspector_project_source.*` receives `IGuiProjectSessionService`
explicitly, exports one canonical in-memory MVR snapshot, and retains those
exact bytes for resource navigation until the user refreshes.
The GDTF inspection result also retains the existing immutable mode/channel and
wheel-catalog read models as in-memory detail data. Filesystem and owned-byte
entry points populate those models from the same retained `description.xml`;
the GUI only projects them and neither parses XML nor enters an editor session.
The minimal `perastage_inspection_serialization` boundary converts an existing
neutral `Result` to deterministic machine-readable JSON, while the higher-level
`perastage_inspection_report_serialization` boundary composes complete reports
from existing GDTF, MVR, resource, and validation results. Frontends continue to
own presentation formatting. The semantic contract remains neutral to both
concerns, and file readers and format-specific inspection services remain
separate from them.

Resource browsing adapts the authoritative package inventory into neutral entry
descriptors and provides explicit-limit raw reads for filesystem and owned-byte
packages. Reads reject unsafe or ambiguous paths and enforce the caller's limit
both before allocation and while decompressing. Validated local-header offsets
bind central-directory selections to their physical ZIP records. Content kinds
are identified conservatively from bounded prefixes plus filename hints, so
large entries need not be read completely. Prefixes have a fixed 64-byte
per-entry ceiling, which covers the supported signatures and bounds aggregate
payload storage for the classic-ZIP entry-count limit; uncertain or mismatched
payloads remain binary. UTF-8 XML and explicitly supported text can
be previewed without rewriting source content. Embedded GDTF packages in MVR are
acquired through this bounded path and passed to the same GDTF inspection
implementation used for standalone files. Because that authoritative reader is
currently path-based, owned GDTF bytes use the existing automatically removed
`runtime_storage::TemporaryWorkspace`; its internal path is replaced with the
caller identity in diagnostics and is never exposed by the inspection result.
Byte-backed GDTF documents have no standalone filesystem source, while nested
results retain the outer package request and embedded entry identity separately.
Image and model decoding remains the responsibility of later
presentation-specific preview adapters. The Inspector owns one long-lived
managed worker with source and preview request lanes; source work has priority,
and obsolete preview interaction can never supersede a newer source load.
Immutable displayed-package contexts bind the
package kind, filesystem path or shared owned bytes, prepared resources,
inspection result, and cache fingerprint. Source requests invalidate visible
package interaction immediately; source generations and preview
sub-generations independently reject stale results at the worker and GUI event
boundaries. Shutdown updates its wait predicate under the worker mutex before
notifying, so an idle worker cannot miss the wakeup. Joining happens after
releasing that mutex and before the window is destroyed.

Inspector resource presentation deliberately separates three concepts. Core's
`ResourceKind` remains the semantic classification, Source is the textual
representation of either the retained primary document or an exact bounded
selected resource, and Preview is an optional visual representation. In
particular, a validated SVG remains `XmlText`; a toolkit-independent policy
selects XML Source plus SVG Preview only when that classification and a
case-insensitive `.svg` suffix agree. One bounded text read supplies both
representations, while wxWidgets rasterization stays isolated in the bitmap
cache adapter. The source-document state retains primary authority separately
so image, model, unsupported, and failed selections restore it deterministically.

The active-project scene is never traversed by a worker. The GUI-owned adapter
synchronously copies the live `MvrScene` and captures the truss-geometry export
setting without mutation; canonical ZIP serialization, inspection, and package
classification then run from that immutable input on the source worker. The
scene copy is the only residual synchronous current-project stage. Heavy
package bytes and inspection results use shared immutable ownership, including
parent MVR restoration after nested GDTF navigation.

Inspector previews are selection-driven and do not decode entries while the
package model is populated. Automatic reads are capped at 16 MiB for images,
64 MiB for GLB/3DS models, 16 MiB for nested-GDTF preview, and 4 MiB for
text/XML. An explicit embedded-GDTF open has a separate 512 MiB hard ceiling;
known larger entries remain visible but are refused as an Inspector operation,
and unknown sizes remain bounded by the same ceiling. This higher explicit
ceiling accommodates production fixture archives with model assets while still
preventing archive metadata from selecting an unbounded allocation. The
existing GUI bitmap cache is the sole owner of its 4096-pixel decoded-image
dimension limit, while the Inspector configures a 64 MiB bitmap cache budget.
Path-only loaders receive owned bytes through an
`inspector-preview` `runtime_storage::TemporaryWorkspace`; it is owned by the
preview panel, removed on replacement/destruction, never changes the source,
and never appears in semantic diagnostics. No resource is permanently
extracted. XML above 2 MiB initially uses a clearly labelled bounded view while
retaining the exact text for copy and an explicit complete-load action. These
preview limits describe GUI resource use only and never become standards
validation findings. GLB/3DS geometry-only parsing runs on the preview worker
with texture decoding disabled, and only prepared CPU geometry reaches the GUI
and OpenGL panel. GDTF fixture-model parsing remains GUI-thread work because the
existing archive/cache loader is not thread-safe; its automatic input is
therefore constrained by the 16 MiB nested-GDTF preview cap.

`perastage_inspection_core` is the minimal non-GUI link boundary. This
static library owns only `core/inspection/inspection_contract.cpp`, publishes
the `core/` include root, and requires C++20; it has no wxWidgets or other
third-party link dependency. The focused inspection services and
`InspectionContract` executable link this one production implementation. The
normal GUI application links the focused GDTF, MVR, and resource inspection
targets required by its Inspector workspace. The CLI links the same services
independently; neither frontend calls the other or owns parsing, archive reads,
validation, or mutation.
The focused target is intentionally not a general Core library or a migration
of MVR and GDTF code. It may grow only when a concrete inspection service
requires another reviewed dependency.

The minimal Inspection Core stays standard-library-only because readers have
separate XML/archive dependencies and application importers add project/UI
integration. It must not acquire Viewer or App dependencies, or become a broad
Core library without a concrete service need and reviewed dependencies.

`perastage_gdtf_read` is the single production owner of
`gdtf_archive_reader.cpp`, `gdtf_description_reader.cpp`, and
`gdtf/editor/gdtf_document.cpp`. The application and focused inspection services
consume that same implementation independently. Its public headers expose only
standard C++ and immutable GDTF read models. tinyxml2 and wxWidgets base/archive
facilities remain private, using `wx::base` with config packages and base-only
flags from the selected `wx-config` on Linux and macOS.

`perastage_inspection_gdtf` composes package inventory with
`LoadGdtfDocument`. The existing archive snapshot exposes the selected raw
`description.xml`, its actual entry path, and canonical or compatibility state.
The existing description snapshot supplies fixture metadata, revisions,
physical values, DMX modes, wheels, slots, and resource references. Original
reader diagnostics remain in those snapshots and are also adapted to ordered
neutral inspection diagnostics with stable `gdtf.archive.*` and
`gdtf.description.*` codes.

The service is strictly read-only and follows stable DIN SPEC 15800:2022-02
behavior from the official specification `main` branch. It does not rewrite
XML, canonicalize archives, invoke editor sessions, retrieve resource bytes, or
add another GDTF parser. Semantic coverage is limited to the established read
models; standards validation and resource preview have separate owners.

`perastage_inspection_serialization` is a C++20 static library that owns only
`core/inspection/inspection_json_serializer.cpp`. Its standard-C++ public API
accepts the semantic `Result` and returns a compact JSON string; the vendored
JSON implementation remains private. The library depends on
`perastage_inspection_core` and has no GUI, App, viewer, XML, networking, or
graphics dependency. It serializes in-memory results and does not inspect or
reparse files.

`perastage_inspection_report_serialization` is the frontend-neutral C++20
composition boundary for complete GDTF and MVR reports. It depends on the
minimal base serializer and the focused GDTF, MVR, and resource Inspection
targets, reuses the base serializer's private diagnostic/path/token helpers,
and adds only already-structured package, validation, document, and snapshot
facts. It never reparses packages or XML, exposes no JSON implementation or
GUI/wx type, and keeps complete reports from broadening the minimal base serializer.

`perastage_inspection_package` is the C++20 static boundary for
read-only `.gdtf` and `.mvr` classification and ZIP metadata inventory. Its
public model is standard C++ and builds on the neutral inspection contract;
the standard-library raw ZIP metadata target is its only implementation
dependency. Package inspection does not depend on wxWidgets.
The service does not start a GUI lifecycle, create a project, extract package
contents, or read XML payloads. Direct `.xml` and generic `.zip` inputs are
unsupported and are never guessed from their contents.

Inventory entries retain the decoded archive spelling for display. A separate
forward-slash normalized identity is published only when the entry is a safe
archive-relative path. Empty, absolute, drive/root-style, colon-bearing, and
`..` traversal identities (including traversal written with backslashes) stay
visible but are marked unusable and diagnosed. Classic ZIP field widths bound
metadata allocations; ZIP64 and multi-disk packages receive the neutral fatal
`package.unsupported_zip_structure` implementation diagnostic rather than
being mislabeled as malformed or as standards violations. Sentinel values are
never interpreted as offsets. This
layer exposes only the presence of the canonical
root filename; GDTF and MVR XML semantics belong to the shared readers,
and conformance findings belong to the validation layer.

The standard-library-only `perastage_archive_zip_directory` static library owns
bounded classic-ZIP directory mechanics shared by package inspection and the
existing GDTF reader: EOCD lookup with comment validation, single-disk and
ZIP64 checks, central-directory bounds, local/central filename consistency,
raw filename bytes, UTF-8 flags, and entry order. GDTF retains its compatibility
decoding and diagnostics. The stricter layout-package preflight remains local
because its canonical-path and entry-count policy is specific to layout
imports. Reusing directory mechanics must preserve that stricter behavior.

The shared ZIP metadata entries also provide classic uncompressed size and
portable directory identity from a trailing `/`. Package inspection builds its
inventory directly from that metadata, without wx streams or positional
pairing. The MVR importer remains a separate extraction/import workflow.

`perastage_mvr_read` is the single production owner of MVR package
acquisition, read orchestration, scene-node reading, resource resolution, and
reference resolution. Every `MvrImporter` mode delegates General Scene
Description traversal to that seam. The focused
`perastage_mvr_import_application_read` adapter supplies local/embedded GDTF
metadata, mode resolution, truss definitions, geometry bounds, legacy layer
reconciliation, and dummy-hoist lookup to the same parser. Project application,
conflict dialogs, download workflow, and dictionary application remain outside
the shared parser in the application-facing importer. The reusable target's
remaining wxWidgets dependency is limited to base memory and ZIP streams used
by the established safe extraction implementation. The narrow
`perastage_mvr_core_support` and `perastage_mvr_scene_model` targets are declared
by Core and Models respectively, so the shared reader links their single
production implementations without directly registering foreign module
sources or creating a broad general-purpose Core library.

The shared read context returns neutral conflict and manually authored category
facts, exact authored layer UUID membership for every parsed node, and each
layer's direct authored child UUIDs. This preserves duplicate layer names and
nested group inheritance without mutating application scene models solely for
inspection. The application importer applies dictionary persistence and the
existing GDTF Share conflict/download workflow after reading; inspection requests no
such side effects.

Fixture-category inference, final GDTF mode compatibility resolution, and
synthetic default-layer creation are application enrichment. They run after the
shared structural read with per-type and per-resource caches, so the Inspector
does not open nested GDTFs or synthesize application scene state.

MVR package acquisition reuses `perastage_runtime_storage`, the production
owner of `TemporaryWorkspace` and `SceneResourceLease`; inspection does not
define a parallel temporary-directory lifetime. The Inspector supplies a
restricted package-only read environment to the shared parser, while application
imports supply the richer adapter without changing XML traversal ownership.

`perastage_inspection_mvr` is a read-only consumer whose public API uses
standard C++ filesystem paths or owned bytes. It composes the MVR package
inventory with one acquired package passed to `perastage_mvr_read`, avoiding a
second extraction. Byte inputs use the same bounded central-directory reader
directly from memory, so they retain the same ordered entry names, sizes, path
safety, and package diagnostics as filesystem inputs. Results own the selected
scene-description entry and original XML,
embedded GDTF entry paths, safe scene resource references, metadata, and
deterministically ordered node descriptors and counts, including authoritative
layer and group child relationships. Symdefs use a dedicated descriptor whose
ordered values are retained packaged geometry references rather than child
UUIDs; those references also participate in the resource inventory. Temporary
extraction paths and leases are not exposed.

The service never invokes project application, replacement, merge, dialogs,
viewers, or configuration mutation. Inspection disables dictionary application
and dummy fallback, and it has no dependency on GDTF Share or download
workflows, so missing referenced GDTFs remain read-only diagnostic context.
No wxWidgets type crosses the inspection API and no `wxApp` or GUI lifecycle
is required. The standalone inspection test links only the production
inspection/read targets and normal non-GUI dependencies. Broader schema
validation, nested resource byte
browsing, and characterization beyond the focused service contract remain
outside this boundary.

The standards baseline is the current stable DIN SPEC 15800:2022-02 / GDTF
1.2 and DIN SPEC 15801:2023-12 / MVR 1.6 package format. Package inspection
uses stable package identity facts; semantic validation has a separate owner.

Inspection JSON schema version `1` contains `schema_version`, `request`,
`success`, `worst_severity`, and `diagnostics`. The request contains
`source_path`; diagnostics contain `severity`, `domain`, `classification`,
`code`, `message`, and an optional `location`. Locations may contain
`source_path`, `package_entry`, `xml_path`, `line`, and `column`, with absent
optional values omitted. An absent diagnostic location is omitted, an empty
result has a null `worst_severity`, and `diagnostics` is always an array in the
semantic insertion order. Filesystem paths are emitted as UTF-8 with generic
`/` separators, and compact output is deterministic for the same result.

The stable severity tokens are `information`, `warning`, `error`, and `fatal`;
domain tokens are `input`, `package`, `xml`, and `content`; classification
tokens are `general`, `standards`, and `compatibility`. Stable field names and
tokens do not silently change within a schema version. Removing or renaming a
field, changing its type, or reinterpreting its meaning requires a new schema
version. Deliberate additive optional fields may retain the version when
existing meanings remain unchanged. Object key order and presentation
formatting are not schema guarantees, and CLI text output is outside the JSON schema.

## Library convention

- Scene-object presets live in `library/scene_objects/`.
- Any new code/path references must use `scene_objects` (underscore), not `scene objects`.

## GUI architecture references

- Keyboard shortcut routing and scope rules are documented in `gui_shortcut_architecture.md`.
- Storage source-of-truth and runtime precedence are documented in `docs/developer/storage_policy.md`.

### XML and standards validation layers

Inspection reports three independent validation layers. XML well-formedness answers only whether the retained source is parseable; XSD validation checks the applicable embedded schema; semantic/interoperability validation reuses facts from the established TinyXML2-backed GDTF and MVR readers. Package and compatibility diagnostics remain independently classified. Neither tolerant reading nor XSD success proves complete standards or interoperability compliance.

`perastage_inspection_validation` is the sole owner of libxml2. Its private adapter uses libxml2 only as an XSD 1.0 engine and exposes neutral Core values; no libxml2 or wxWidgets type crosses its public API. TinyXML2 and the existing production readers remain authoritative for semantic data. Context-local resource loaders deny external XML and schema resources, and parsing disables external entities, network access, and system catalogs.

The schema layer embeds exact official `mvrdevelopment/tools` files from revision `e199c6ed635de23cb5ebf9654ee54a358775a065`: `gdtf.xsd` for GDTF 1.2 / DIN SPEC 15800:2022-02 and `mvr.xsd` for MVR 1.6 / DIN SPEC 15801:2023-12. Their stable specification basis is `mvrdevelopment/spec@098d3791f77f0895bd859adf01864b4826e2006f`. Provenance files record source paths and hashes, while an offline guard detects changes. The official MVR XSD is intentionally backwards-compatible, so version-specific provider, Fixture `ChildList`, and `FixtureTypeId` rules remain semantic checks in the existing reader path. XSD 1.0 limitations, package constraints, resource existence, and graph/reference integrity remain semantic or package responsibilities.

## Runtime 3D model detail

Core owns cross-cutting viewport interaction preferences alongside shared
selection movement settings. GUI and both viewers consume that neutral policy
without introducing viewer or UI implementation dependencies into Core.

Core owns GUI-independent model-detail configuration and triangle/error budgets.
Viewer3D owns immutable runtime representations and a per-viewer mesh cache;
GUI owns the category-based Preferences container and focused settings pages.
See [Runtime model detail and Preferences](model_detail.md) for policy values,
simplification, invalidation, and preserved settings.

The MVR import GDTF workflow links `perastage_gdtf_matching` (standard-library-only
ranking) and `perastage_gdtf_resolution` (typed choices, fallback/application state
and shared package validation). Native queue controls, credentials and catalog
selection stay in `gui/`; the existing download queue modal boundary is reused.
See [MVR GDTF resolution](technical-notes/mvr_gdtf_download_queue.md) for ownership,
automatic selection policy and the proposed headless adapter boundary.

## Layout mutation history

`LayoutManager` owns candidate validation and semantic change detection for page
and element operations. Successful changes publish one departing project snapshot
through `ConfigManager`; unchanged or rejected candidates publish nothing. GUI
frame movement remains a local preview until release, and cancellation discards
it. Passive 2D viewport persistence explicitly skips history publication.

Project history captures Layout metadata presence and image registry bytes along
with scene, selection, layer state, label overrides and fixture symbols. History
restoration bypasses startup default loading and resource re-reading, preserving
absent metadata and explicitly empty collections without rewriting configuration.

## Fixture label typography

Core's GUI-independent `typography/` boundary owns the existing TrueType metrics
reader, WinAnsi encoding and bounded text composition. It produces advances,
line boundaries and baselines in explicit scene metres or paper points. Bundled
PerastageSans resolves to Noto Sans 2.004 regular/bold; composed fixture labels
use those exact files, independently positioned glyphs and no backend kerning.
Viewer2D owns fixture field resolution and composition into existing canvas text
commands. GUI owns the editor's draft and symbol preview; Viewer3D submits the
same composed positions to NanoVG and capture, and PDF embeds the resolved fonts.

Version-2 `presentation` fields extend existing PSTG label overrides with an independent fill and an outward contour radius in scene metres. Bundled TrueType contours (including composite accents) feed the NanoVG vector halo; screen fill uses the existing NanoVG font atlas at each composed glyph position and baseline. Polygon antialias fringes overfill subpixel stems, so vector fill at small zoom does not match legacy text coverage. No backend relayout or advance calculation is used. PDF strokes the embedded glyphs with twice the authored radius as line width. Display-only stroke resolution maps the 0.004 m reference radius to one framebuffer pixel, with proportional custom weights and no scene-zoom factor. The main viewer uses framebuffer coordinates (NanoVG ratio 1); the editor preview divides by its NanoVG device pixel ratio to avoid double DPI scaling. Composition/capture retain the authored physical radius and positioned geometry; PDF never uses this display policy. Unedited legacy text retains its eight-offset rendering. Version 1 remains readable with contrasting outline defaults. Opening the editor never reserializes metadata. Legacy and edited names share the fixed two-space-delimited-words-per-line rule in `fixture_label_name.h`, before composition; no new wrap setting or serialization is introduced. The adapter accounts for multiline Name height while preserving ID/DMX order. Preview and property controls share the legacy stack adapter; property edits retain unrelated geometry and unrounded offsets. Label-only commits touch document history/dirty state without changing the scene-resource revision; serialized styles invalidate composition and Layout viewport caches. The reserved
`__project_defaults__` entry participates in the same project history and is
excluded from UUID normalization/pruning. Legacy keys remain a read-only adapter
until a field is edited; existing line-spacing and Layout compensation constants
remain intact. Layout free text, legends, tables, 3D labels and the old global PDF
metrics compatibility API remain legacy consumers. The next integration is shared
composition for Layout free text, with explicit migration and wrapping tests;
this change does not claim preview/PDF parity for those paths.
