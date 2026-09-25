# Architecture and repository conventions

This document is the authoritative source for Perastage's architectural
ownership, module responsibilities, and accepted dependency directions. For
the canonical human-readable map of repository paths and root-file roles, see
[Repository Layout](repository_layout.md).

## Top-level layout

<!-- repository-source-module-responsibilities: app=application lifecycle and startup composition; core=shared business logic and services; gui=wxWidgets UI and workflows; models=scene data structures; mvr=MVR interchange; viewer2d=2D rendering and export; viewer3d=3D rendering and loading; viewer_common=shared viewer utilities -->

This stable marker is checked against the canonical source-module inventory in
`repository_structure_baseline.json`; the descriptions below define each
module's architectural responsibility.

- `app/`: wxWidgets application lifecycle and startup composition.
- `core/`: shared business logic and services.
- `gui/`: wxWidgets UI and main window workflows.
- `viewer2d/`: 2D renderer and PDF/export helpers.
- `viewer3d/`: 3D renderer, loaders and render passes.
- `models/`: core scene data structures.
- `mvr/`: MVR import/export modules.
- `viewer_common/`: utilities shared by the 2D and 3D viewers.
- `third_party/`: vendored third-party single-header dependencies (for example `json.hpp`, `stb_easy_font.h`).
- `library/`: bundled runtime content (fixtures, trusses, `scene_objects`, examples).

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

The ORG-023 audit classified the application target's include directories from
actual include spellings and CMake ownership before ORG-024 changed their
declarations:

| Classification | Audited paths | Evidence and ownership |
|---|---|---|
| A: module-owned and formerly redundant in root | `core`, `core/layouts`, `core/print`; `gui`, `gui/mainwindow/controllers`, `gui/mainwindow/ids`; `models`; `mvr`; `viewer2d`, `viewer2d/pdf`; `viewer3d`, `viewer3d/interfaces`, `viewer3d/resources`, `viewer3d/culling`, `viewer3d/labels`, `viewer3d/picking`, `viewer3d/render` | Sources use both unqualified headers such as `logger.h` and nested spellings such as `resources/resource_sync_system.h`. Each complete group was already applied to `${PROJECT_NAME}` by its owning module CMake file, so ORG-024 removed only the duplicate root declarations. |
| B: module-specific without local ownership | None | All audited feature paths had an active owner declaration. `viewer_common` was locally owned even though it was not in the pre-audit root list. |
| C: shared project-wide | `third_party` | The application uses unqualified vendored headers from several modules: `json.hpp` in Core, GUI, MVR, Viewer2D, and Viewer3D, and `stb_easy_font.h` in Viewer3D. The shared application declaration remains in root. |
| D: dependency-provided state | `${_wx_includes}`, `${NANOVG_INCLUDE_DIR}` | Dependency configuration supplies these compatibility variables and application sources consume wxWidgets across modules and NanoVG in the viewers. They remain shared target configuration pending a separate dependency-modernization effort. |
| E: unresolved compatibility exposure | `resources` | No application C/C++ header include currently resolves through this directory; it contains platform resources and runtime assets. The include is retained until the same revision has native Windows, Linux, and macOS build evidence that it is unnecessary. |

Test executables are separate targets and continue to declare their own focused
include requirements in `tests/CMakeLists.txt`; their requirements did not
justify any application-target path. The audit also observed factual
cross-module header use (notably GUI and viewers consuming Core and Models,
Viewer3D consuming MVR data, and GUI consuming Viewer2D/Viewer3D facilities).
That include-directory audit did not itself define direction policy. The
subsequent dependency audit produced the enforced contract below.

All feature modules still contribute to the same `${PROJECT_NAME}` target.
Moving declarations to module CMake files records ownership and reduces root
coupling, but does not create source-local compiler isolation or a new target
boundary. Stronger compile-time isolation would require a future target-level
architecture change.

### Internal module dependency directions

The following contract records the current production source-level includes.
It is not a target-level link graph: all eight modules still contribute to the
single application target. Counts are evidence occurrences from the current
inventory; same-module includes and test sources are excluded.

| Consumer | Accepted providers (evidence count) | Architectural rationale |
|---|---|---|
| `app` | `core` (12), `gui` (2), `viewer3d` (1) | Application composition coordinates lifecycle services, windows, and the existing GDTF cache teardown API. |
| `core` | `models` (46), `mvr` (5), `viewer2d` (4), `viewer3d` (7) | Application services coordinate scene data, interchange, and existing symbol/geometry implementations. |
| `models` | None | Scene data does not include another audited application module. |
| `mvr` | `core` (55), `gui` (3), `models` (9), `viewer2d` (1), `viewer3d` (2) | Interchange uses shared services and scene data plus existing import presentation, label, and geometry facilities. |
| `gui` | `core` (411), `models` (42), `mvr` (15), `viewer2d` (77), `viewer3d` (45), `viewer_common` (5) | UI workflows orchestrate application services, interchange, both viewers, and shared GL utilities. |
| `viewer_common` | `core` (4) | Shared viewer utilities use central preferences and diagnostics. |
| `viewer2d` | `core` (26), `gui` (12), `models` (5), `viewer3d` (8), `viewer_common` (7) | 2D rendering consumes scene/services, shared GL support, existing 3D types, and UI command identifiers. |
| `viewer3d` | `core` (70), `gui` (13), `models` (24), `viewer2d` (13), `viewer_common` (6) | 3D rendering consumes scene/services, shared GL support, 2D render interfaces, and existing UI status facilities. |

The current graph contains intentional-for-current-architecture cycles between
`core` and each of `mvr`, `viewer2d`, and `viewer3d`; between `gui` and each of
`mvr`, `viewer2d`, and `viewer3d`; and between `viewer2d` and `viewer3d`.
In particular, renderer-to-GUI includes, `mvr -> gui`, and Core's viewer
includes are visible technical-debt directions rather than a proposed layering
model. ORG-025 accepts them because they exist; removing them requires a
separate behavior-preserving architecture change.

`tests/check_module_dependency_directions.py` resolves quoted includes through
source-relative paths and the application's ordered include roots, then rejects
ambiguous project headers and directions outside this reviewed set. A legitimate
new direction requires architectural review and a coordinated update to this
section and the check's explicit `ACCEPTED_DIRECTIONS`; the check never blesses
new edges automatically.

## Application bootstrap ownership (ORG-030–033)

ORG-030 audited the former root-owned bootstrap and selected `app/` as the
application-composition owner. ORG-031 through ORG-033 implement that boundary:
`app/perastage_app.h` owns the `MyApp : wxApp` declaration and lifetime state,
while `app/perastage_app.cpp` owns lifecycle methods and private bootstrap
helpers. Root `main.cpp` is now limited to including the App declaration and
invoking `wxIMPLEMENT_APP(MyApp)`.

The extraction preserves the audited startup sequence and semantics: launch-CWD
capture, CLI and platform open routing, two-stage deferred startup resolution,
localization and fallback warning, diagnostics and crash handling, splash and
main-window composition, metrics, platform hooks, and ordered shutdown. App
depends downward on `core` (12 include occurrences), `gui` (2), and `viewer3d`
(1). The Viewer3D direction remains intentional composition debt because App
coordinates the existing GDTF cache shutdown API. No lower-level module may
include App headers or depend upward on application lifecycle policy.

`app/CMakeLists.txt` explicitly contributes the App declaration and
implementation to `${PROJECT_NAME}` and exposes only its local include directory
to that target. Root CMake registers `app/` after lower-level source modules;
its `add_executable` continues to own only `main.cpp` and generated build
information. The repository baseline, dependency-direction inventory, bootstrap
ownership check, startup-publication guard, and localization catalog scanner
enforce this implemented boundary.

The detailed ORG-030 audit rationale remains the extraction contract: App owns
wxWidgets lifecycle, startup selection and external-open queuing, localization
orchestration, diagnostics, splash sequencing, `MainWindow` construction, and
teardown. Core retains reusable services, GUI retains visible startup
publication, and Viewer3D retains cache implementation. Startup argument/path
selection may be split internally in a future focused change, but ORG-034 and
later organization work is outside this implementation.

## Tool and inspection contract

Core owns the neutral request, structured result, and diagnostic types under
`core/inspection/`. The contract is GUI-independent and read-only: it identifies
a filesystem input and preserves ordered diagnostics with stable technical
identifiers, severity, domain, classification, and optional source metadata.
Future CLI, Inspector GUI, Console, and other adapters consume these structured
results. Serialization and presentation are outside this semantic contract.
The separate `perastage_inspection_serialization` boundary converts an existing
result to deterministic machine-readable JSON, while frontends own presentation
formatting. The semantic contract remains neutral to both concerns, and file
readers and format-specific inspection services remain separate from it.

`perastage_inspection_core` is the first minimal non-GUI link boundary. This
static library owns only `core/inspection/inspection_contract.cpp`, publishes
the `core/` include root, and requires C++20; it has no wxWidgets or other
third-party link dependency. The focused inspection services and
`InspectionContract` executable link this one production implementation. The
application will link it when a production inspection frontend is introduced.
The focused target is intentionally not a general Core library or a migration
of MVR and GDTF code. It may grow only when a concrete inspection service
requires another reviewed dependency.

The dependency audit supporting this boundary found that the API-010 header
and implementation use only the C++ standard library. The GDTF description
reader and its snapshots are presentation-independent but use tinyxml2, while
`GdtfDocument` composes that reader with the archive reader. The archive reader
still uses wxWidgets base file and ZIP streams plus the Core filesystem-to-wx
path adapter. On the MVR side, scene-node parsing, import result types,
reference resolution, and project-application separation provide reusable
read-side pieces, but package acquisition accepts `wxInputStream`, and the
current importer also contains wxWidgets UI and application interactions.
Adding those paths now would therefore introduce XML/archive dependencies or
incorrectly pull GUI/application/model integration into this standard-library
foundation. Viewer and App sources are not dependencies of this target.

INS-110 adds `perastage_gdtf_read` as the single production owner of
`gdtf_archive_reader.cpp`, `gdtf_description_reader.cpp`, and
`gdtf/editor/gdtf_document.cpp`. The application and the focused
`perastage_inspection_gdtf` target independently consume that implementation;
the inspection service is not linked into the application until a real
frontend consumes it. The read target keeps tinyxml2 and wxWidgets base/archive
facilities private, using `wx::base` with config packages and base-only flags
from the already-selected `wx-config` on Linux and macOS. Its public headers
expose only standard C++ and immutable GDTF read models. The application's
existing aggregate wx GUI dependency list remains unchanged.

`perastage_inspection_gdtf` composes INS-100 package inventory with
`LoadGdtfDocument`. The existing archive snapshot exposes the selected raw
`description.xml`, its actual entry path, and canonical or compatibility state.
The existing description snapshot supplies fixture metadata, revisions,
physical values, DMX modes, wheels, slots, and resource references. Original
reader diagnostics remain in those snapshots and are also adapted to ordered
API-010 diagnostics with stable `gdtf.archive.*` and
`gdtf.description.*` codes.

The service is strictly read-only and follows stable DIN SPEC 15800:2022-02
behavior from the official specification `main` branch. It does not rewrite
XML, canonicalize archives, invoke editor sessions, retrieve resource bytes, or
add another GDTF parser. Semantic coverage is limited to the established read
models; deeper validation and preview are future responsibilities.

`perastage_inspection_serialization` is a C++20 static library that owns only
`core/inspection/inspection_json_serializer.cpp`. Its standard-C++ public API
accepts the semantic `Result` and returns a compact JSON string; the vendored
JSON implementation remains private. The library depends on
`perastage_inspection_core` and has no GUI, App, viewer, XML, networking, or
graphics dependency. It serializes in-memory results and does not inspect or
reparse files.

`perastage_inspection_package` is the INS-100 C++20 static boundary for
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
root filename; GDTF and MVR XML semantics remain deferred to INS-110 and
INS-120, and conformance findings remain deferred to INS-130.

The standard-library-only `perastage_archive_zip_directory` static library owns
bounded classic-ZIP directory mechanics shared by package inspection and the
existing GDTF reader: EOCD lookup with comment validation, single-disk and
ZIP64 checks, central-directory bounds, local/central filename consistency,
raw filename bytes, UTF-8 flags, and entry order. GDTF retains its compatibility
decoding and diagnostics. The stricter layout-package preflight remains local
because it combines these mechanics with layout-specific canonical-path and
entry-count policy; migrating it is intentionally deferred to avoid changing
layout import behavior in INS-100.

The shared ZIP metadata entries also provide classic uncompressed size and
portable directory identity from a trailing `/`. Package inspection builds its
inventory directly from that metadata, without wx streams or positional
pairing. The MVR importer remains a separate extraction/import workflow.

INS-120 adds `perastage_mvr_read` as the single production owner of MVR package
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
standard C++ filesystem paths or owned bytes. It composes the INS-100 MVR
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
1.2 and DIN SPEC 15801:2023-12 / MVR 1.6 package format. INS-100 uses only
stable package identity facts and does not implement future proposals or
semantic validation.

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
formatting are not schema guarantees, and future CLI text output is outside the
JSON schema.

## Library convention

- Scene-object presets live in `library/scene_objects/`.
- Any new code/path references must use `scene_objects` (underscore), not `scene objects`.

## GUI architecture references

- Keyboard shortcut routing and scope rules are documented in `docs/developer/../developer/gui_shortcut_architecture.md`.
- Storage source-of-truth and runtime precedence are documented in `docs/developer/storage_policy.md`.
