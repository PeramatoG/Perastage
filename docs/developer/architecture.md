# Architecture and repository conventions

This document defines the expected directory conventions for Perastage.

## Top-level layout

- `core/`: shared business logic and services.
- `gui/`: wxWidgets UI and main window workflows.
- `viewer2d/`: 2D renderer and PDF/export helpers.
- `viewer3d/`: 3D renderer, loaders and render passes.
- `models/`: core scene data structures.
- `mvr/`: MVR import/export modules.
- `third_party/`: vendored third-party single-header dependencies (for example `json.hpp`, `stb_easy_font.h`).
- `library/`: bundled runtime content (fixtures, trusses, `scene_objects`, examples).

## Third-party convention

- Keep vendored code under `third_party/` only.
- Do not place third-party libraries inside feature modules (`core/`, `gui/`, `viewer*`, etc.).
- Prefer package-managed dependencies for compiled libraries; use `third_party/` for vendored sources/headers only.

## CMake convention

- Root `CMakeLists.txt` owns project options, principal target creation, shared target configuration, and module orchestration. Focused modules own dependency discovery (`PerastageDependencies.cmake`), localization (`PerastageLocalization.cmake`), runtime staging (`PerastageRuntimeStaging.cmake`), installation (`PerastageInstall.cmake`), and packaging (`PerastagePackaging.cmake`).
- `cmake/platform/PerastagePlatform.cmake` dispatches target-level platform configuration to separate Windows, macOS, and Linux owners after the application target exists. Linux desktop, MIME, and icon integration remains installation configuration rather than target configuration.
- Every top-level application source module contributes its explicit source list using its local `CMakeLists.txt` and `target_sources(${PROJECT_NAME} ...)`.
- `docs/developer/repository_structure_baseline.json` is the authoritative machine-readable contract for source-module classification and CMake registration.
- Avoid recursive or wildcard project-source discovery; list files explicitly.
- Keep include directories close to the module that owns them.

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
ORG-025 will evaluate dependency direction; this audit defines no direction
policy.

All feature modules still contribute to the same `${PROJECT_NAME}` target.
Moving declarations to module CMake files records ownership and reduces root
coupling, but does not create source-local compiler isolation or a new target
boundary. Stronger compile-time isolation would require a future target-level
architecture change.

### Internal module dependency directions

The following contract records the current production source-level includes.
It is not a target-level link graph: all seven modules still contribute to the
single application target. Counts are evidence occurrences from the ORG-025
audit; same-module includes and test sources are excluded.

| Consumer | Accepted providers (evidence count) | Architectural rationale |
|---|---|---|
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

## Application bootstrap ownership audit (ORG-030)

ORG-030 audits the current root `main.cpp`; it does **not** move bootstrap code,
create a module, change `MyApp`/`wxIMPLEMENT_APP`, or begin ORG-031, ORG-032, or
ORG-033. The intended owner for a later extraction is a top-level `app/` module.
Bootstrap is application composition: it may depend on Core services and GUI
types, whereas neither `core/` nor other GUI-independent modules may depend on
`app/` or acquire wxWidgets application-lifecycle responsibilities. This makes
`app/` a clearer owner than `core/` or `gui/`.

### Current responsibilities and future owners

The following classification is based on the current implementation and records
the ordering, platform, and repository constraints that a later change must
preserve. "App" means the proposed `app/` module, not an existing directory.

| Responsibility | Current owner and dependencies | Intended owner | Constraints and later references |
|---|---|---|---|
| Process/application entry wiring | `wxIMPLEMENT_APP(MyApp)` supplies the wxWidgets-generated process entry point. | Minimal root `main.cpp`. | Keep exactly one compiled root entry point and preserve wxWidgets platform entry semantics. Root `CMakeLists.txt`, the structure baseline and its fixture tests currently require this path. |
| `wxApp` type and lifecycle declarations | The `MyApp : wxApp` declaration lists `OnInit`, `OnExit`, event/exception hooks, macOS callbacks, and private startup methods/state. | App, in a focused public `app/perastage_app.h`; implementations belong in App `.cpp` files. | The root should include the header solely to instantiate the application. Non-macOS compatibility declarations must remain so callback logic stays buildable/testable on all platforms. |
| Startup sequencing | `MyApp::OnInit` creates metrics, configures debug behavior and process locale, captures the launch CWD, publishes app/vendor names, changes to the executable directory, initializes images/appearance, diagnostics, preferences/localization, splash/library bootstrap, and the main window before scheduling startup-open resolution. | App bootstrap implementation. | Preserve the exact sequence, especially capturing launch CWD before changing it; diagnostics before user-facing windows; configuration before localization; splash before library bootstrap; and deferred open resolution after window construction. |
| Startup project/MVR resolution | `GetStartupPathFromArgs` accepts the first `.pstg` (via `ProjectUtils::PROJECT_EXTENSION`) or `.mvr` argument, resolves a relative argument against the captured launch CWD, and `FinalizeStartupOpenResolution` selects macOS explicit open, then CLI, then last project, then empty project. | App; path parsing/selection should become a separate, narrowly testable App helper because it is policy rather than UI presentation. | Preserve extension case folding, quote removal, UTF-8/filesystem conversions, precedence, `clearLastProject` values, two nested `CallAfter` calls that allow a macOS event to arrive, and the single queued `EVT_PROJECT_LOADED`. Packaging associations feed this input but do not depend on the source path. |
| External-open routing | `HandleExternalOpenPath`, `StorePendingStartupExternalOpenPath`, `ConsumePendingStartupExternalOpenPath`, `ConsumePendingExternalOpenPath`, and `QueueProjectLoadedEvent` capture startup requests, queue requests while no top window exists, prevent duplicate initial loads, and later delegate to `MainWindow::EnqueueExternalOpenPath`. | App owns pre-window/lifecycle routing and state; GUI continues to own the ready-state/deferred-open pipeline in `MainWindow`. | Preserve GUI-thread `CallAfter`/`wxQueueEvent`, weak-window lifetime checks, request order, most-recent startup request semantics, and the atomic startup/load gates. Audit note: `ConsumePendingExternalOpenPath` currently has no caller; later work must not silently invent draining behavior during a move. |
| macOS open callbacks | `MacOpenFile`, `MacOpenFiles`, and `MacOpenURL` convert wx strings, log safely, normalize, and enter the shared external-open route. | App lifecycle implementation. | Overrides apply only on `__WXOSX__`; declarations remain available elsewhere. Preserve ordered multi-file dispatch and LaunchServices/Finder timing. macOS bundle configuration is in `cmake/platform`, but does not name `main.cpp`. |
| URI/path bootstrap helpers | `DecodeFileUriToPath`, `NormalizeExternalOpenPath`, `ToLowerAscii`, and the local UTF-8 conversion in `GetStartupPathFromArgs` decode `file://`, normalize separators/absolute paths, and compare extensions. | Initially private App implementation; extract pure parsing/selection portions into a testable App helper when useful. Shared encoding primitives may use existing Core `filesystem_path_utils`, but wx URI and launch policy must not move into Core. | Preserve macOS authority/slash handling, deliberate Windows avoidance of `wxFileName::Normalize`, filename-only diagnostics, and error fallbacks. |
| Localization startup | `OnInit` calls `platform::EnsureProcessTextLocale`, reads `ui_language` from `ConfigManager`, initializes `LocalizationManager`, records timing and logs fallback; `ShowLocalizationFallbackWarningIfNeeded` schedules one Spanish-catalog warning after window creation. | App orchestration; locale and localization implementations remain in Core, warning presentation remains an App concern. | Preserve process-locale-before-config ordering, locale-independent numeric behavior, one-shot warning timing and exact translatable messages. The localization scanner and POT/complete PO source references currently name `main.cpp`. |
| Diagnostic logging and crash handling | `OnInit` initializes `DiagnosticLogger` then `CrashHandler` and logs build/locale data. `OnExit` logs shutdown, calls `ShutdownGdtfCache`, marks runtime teardown, invokes `wxApp::OnExit`, then closes the logger. | App orchestration; implementations remain `core/diagnostics` and the Viewer3D GDTF cache owner. | Initialization must precede window construction and exception reporting. Teardown order must remain cache, crash-handler teardown marker, wx shutdown, final logger shutdown. |
| Exception handling and event context | `FilterEvent` stores the last wx event summary. `LogExceptionWithStack`, `OnExceptionInMainLoop`, and `OnUnhandledException` log/report standard, allocation, and unknown exceptions; recoverable standard main-loop exceptions return `true`. | App lifecycle implementation; stack formatting can be a private diagnostics adapter, while logger/crash report mechanics remain Core diagnostics. | Preserve return behavior and last-event reporting. The `wxStackWalker` trace is Windows-only and exception hooks must retain wx lifecycle signatures. |
| Application startup state | `MyApp` stores last-event text, atomic load/resolution gates, the explicit startup path, FIFO later-open paths, localization-warning state, shared metrics, and resolution start time. | App, preferably split by responsibility into bootstrap/open-routing state rather than exposed globally. | State must live for the wx application lifetime. Keep atomics where callbacks can cross delivery boundaries and keep `startup::Metrics` shared with `MainWindow`. |
| Splash interaction | `OnInit` calls `SplashScreen::Show` and sets the library-bootstrap, main-window, and last-project messages. Final hiding/publication is driven by `MainWindow` startup composition. | App owns startup progress sequencing; GUI retains splash rendering and final publication. | Do not show/maximize the window from App. `tests/check_startup_window_publication.sh` inspects root `main.cpp` directly and must follow the implementation path later. |
| Main-window creation and top-window publication | `OnInit` constructs `MainWindow(app::kName, nullptr, startup_metrics_)` and immediately calls `SetTopWindow`; `MainWindow::PublishInitialMainWindow` later updates layout, maximizes, and shows it. | App composes/sets the wx top window; GUI owns construction internals and authoritative visible publication. | Preserve top-window availability for external-open routing, weak references, and the hidden-until-composed invariant. |
| Last-project loading | `OnInit` reads `ProjectUtils::LoadLastProjectPath`; `FinalizeStartupOpenResolution` uses it only if no explicit startup request wins and emits an empty-load event otherwise. | App selection policy; persistence remains Core `ProjectUtils`. | Read at the current point, preserve precedence and whether last-project state is cleared. Do not merge this with `MainWindow`'s separate user workflow. |
| Startup metrics | `OnInit` creates `startup::Metrics` and records user-config/localization and main-window construction durations; finalization records delayed path-resolution duration. | App records orchestration spans; the neutral metrics data type remains Core and is passed to GUI consumers. | Preserve clock boundaries and shared ownership; extraction must not alter what time is included. |
| Platform startup and debug behavior | `OnInit` uses `ConfigureWindowsDebugHeapLeakCheck` under MSVC Debug, sets wx dark-mode options, and changes CWD; `OnExit` optionally resets `ConfigManager` for leak reporting. | App, with platform-only helpers private to its implementation. | Preserve `PERASTAGE_CRT_LEAK_CHECK=1`, compile guards, MSVC CRT calls, wx version guard, all-platform system option, and shutdown reset timing. Platform CMake retains subsystem/bundle/resource ownership. |
| Library bootstrap and cache teardown | `OnInit` invokes `ProjectUtils::RunStartupLibraryBootstrap`; `OnExit` invokes Viewer3D's `ShutdownGdtfCache`. | App coordinates calls; Core `ProjectUtils` and Viewer3D retain the underlying responsibilities. | Calls stay inside the established splash/diagnostic lifetime. This existing Viewer3D dependency is composition debt and must not be pushed into Core. |

### Target boundary and dependency shape

After the later extraction, root `main.cpp` should contain only the include of
the App-owned `wxApp` declaration and `wxIMPLEMENT_APP(MyApp)` (plus the license
header). `MyApp` needs its own header because the registration macro requires a
complete named type at the root entry point; its state and non-override helpers
remain private. App implementation files may depend downward on wxWidgets,
`core/` configuration, filesystem, localization, platform-locale, startup
metrics and diagnostics services, `gui/` (`MainWindow` and `SplashScreen`), and
the existing Viewer3D cache shutdown API. GUI-independent modules must not
include the App header or depend upward on bootstrap policy.

Startup argument/path selection is the strongest candidate for a separate
testable App helper: its inputs and precedence can be explicit without owning a
window. wx event dispatch, lifecycle callbacks, diagnostic event capture, and
the fallback dialog should remain private implementation details. A later
design may narrow the direct Viewer3D teardown dependency through an
owner-provided lifecycle interface, but ORG-030 neither introduces that
interface nor changes behavior.

When `app/` is actually created, it must have an explicit `app/CMakeLists.txt`
using `target_sources`; root CMake must add the subdirectory while retaining
only `main.cpp` and generated build information in `add_executable`. No globbing
or broadened include path is warranted. The module-direction contract must add
App as a composition-layer consumer, not as a provider to Core/GUI/viewers.

### References that later implementation must update

- Root `CMakeLists.txt` explicitly compiles `main.cpp`. The structure baseline
  classifies it as the only root application source and compiled entry point;
  `tests/check_repository_structure_baseline.py` enforces those lists and local
  module registration, while its fixture suite embeds `main.cpp` and the
  current module set. The minimal root file remains valid, but adding `app/`
  requires coordinated baseline, fixture, root-registration, and module-CMake
  updates rather than weakening the guard.
- `tests/check_startup_window_publication.sh` reads `main.cpp` to reject direct
  `Show`/`Maximize` calls and reads `gui/mainwindow_startup_splash.cpp` for the
  authoritative publication sequence. It must inspect the App implementation
  after the move while continuing to protect the same invariant.
- `scripts/localization_catalog.py` explicitly includes `main.cpp` in its audit
  set and identifies a representative root splash message. The POT and Spanish
  and Simplified Chinese complete catalogs carry `#: main.cpp` locations for
  the three splash messages and two fallback-warning strings. Source scanning,
  representative wording, catalog regeneration, and COMPLETE-catalog checks
  must be updated together after the strings move.
- `docs/developer/repository_layout.md` and `perastage_tree.md` call `main.cpp`
  the bootstrap/entry point. This architecture section, the machine-readable
  baseline, and the GDTF editor architecture audit also mention its open-file
  routing. Later documentation must distinguish the minimal root entry from the
  App owner.
- Windows installer file associations invoke the executable with one `.pstg`
  or `.mvr` argument; Linux MIME/desktop integration and macOS bundle/
  LaunchServices configuration likewise constrain observable open behavior.
  They do not inspect or constrain the `main.cpp` path. Current GitHub Actions
  and installer workflows impose executable, bundle, resource, and association
  behavior but contain no direct source-path reference that needs changing.

## Library convention

- Scene-object presets live in `library/scene_objects/`.
- Any new code/path references must use `scene_objects` (underscore), not `scene objects`.

## GUI architecture references

- Keyboard shortcut routing and scope rules are documented in `docs/developer/../developer/gui_shortcut_architecture.md`.
- Storage source-of-truth and runtime precedence are documented in `docs/developer/storage_policy.md`.
