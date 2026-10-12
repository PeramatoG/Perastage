# Repository Layout

This page is the authoritative human-readable source for Perastage repository
paths, root-file roles, module locations, build/configuration locations, and
repository entry-point ownership. For architectural boundaries and dependency
direction, see [Architecture](architecture.md). For a shorter navigational
view, see [perastage_tree.md](perastage_tree.md).

The machine-readable structural baseline is
[`repository_structure_baseline.json`](repository_structure_baseline.json). The
`RepositoryStructureBaseline` policy test validates its required directories,
root-file roles, build/development entry points, and current CMake ownership
model. The JSON file is the machine-readable structural contract; this page is
its human-readable layout counterpart.

## Documentation source hierarchy

Repository documentation has one owner for each kind of structural fact:

1. [Architecture](architecture.md) owns architectural responsibilities, module
   roles, and accepted dependency directions.
2. [Code Health](code_health.md) owns the durable maintenance and refactoring
   principles, while machine-enforced policy files own volatile thresholds and
   inventories.
3. This page owns the human-readable repository path and entry-point map.
4. [`repository_structure_baseline.json`](repository_structure_baseline.json)
   is the machine-readable structural contract enforced by repository checks.
5. [perastage_tree.md](perastage_tree.md) is a concise navigation aid, not a
   second architecture specification.
6. Specialized guides retain procedural detail: [Build](build.md),
   [Localization](localization.md), [Packaging](packaging.md), and
   [GitHub Actions](github_actions_workflows.md). They defer architectural and
   repository ownership rules to the documents above.

## Top-level structure

<!-- repository-source-modules: app, cli, core, gui, models, mvr, viewer2d, viewer3d, viewer_common -->
<!-- repository-root-source-roles: main.cpp=application_entry_point -->

The marker above provides the deterministic layout mapping for the canonical
source-module inventory; the table below provides the human-readable path role
for every listed module.

| Path | Purpose |
|------|---------|
| `main.cpp` | Minimal wxWidgets application-entry wiring; it includes the App-owned declaration and invokes `wxIMPLEMENT_APP(MyApp)`. |
| `CMakeLists.txt` | Project options, principal target creation, shared target configuration, and build-module orchestration. |
| `CMakePresets.json` | Canonical tracked configure/build presets for supported local development workflows. |
| `app/` | wxWidgets application lifecycle and startup/bootstrap composition, explicitly registered by its local CMake file. |
| `cli/` | Dedicated headless command-line entry point and independently testable grammar, explicitly owned by its local CMake file. |
| `tools/mcp/` | Standalone Rust stdio MCP tool adapter that consumes the stable `perastage-cli` process/JSON contract. |
| `core/capability/` | Semantic operation descriptors, structural request-shape validation, and focused discovery serialization. |
| `core/osc/` | Bounded OSC 1.0 message parsing, loopback UDP transport, and focused semantic Command mapping. |
| `cmake/` | Dependency discovery, CMake helper scripts, generated configuration templates, and platform metadata templates. |
| `core/` | Core logic, import helpers, dictionaries, patching, layouts, printing, persistence, and shared services. |
| `gui/` | wxWidgets windows, dialogs, menus, panels, UI controllers, and user interaction workflows. |
| `models/` | Scene data models for fixtures, trusses, hoists, objects, layers, and placement data. |
| `mvr/` | MVR importer/exporter implementation and MVR-xchange networking modules. |
| `viewer2d/` | 2D plan view rendering, layout capture, PDF/export support, and 2D viewer utilities. |
| `viewer3d/` | 3D rendering engine, camera/view tooling, picking, labels, culling, and render-resource synchronization. |
| `viewer_common/` | Shared viewer utilities used by the viewer modules to avoid duplicating cross-view logic. |
| `library/` | Bundled fixture, truss, object, dictionary, and example runtime data. |
| `resources/` | Icons, images, fonts, Windows resource files, and runtime visual resources. |
| `packaging/` | Installer, desktop integration, package metadata, and staging helpers. |
| `third_party/` | Vendored third-party headers or small external code components kept in the repository. |
| `licenses/` | Third-party license files. |
| `tests/` | Unit tests, architecture guard scripts, and repository consistency checks. |
| `docs/` | User documentation, technical notes, architecture policies, and documentation website assets. |
| `.github/workflows/` | GitHub Actions workflows for CI, packaging, release assets, and platform builds. |

## Build module registration

Application dependency discovery and dependency capability validation are owned
by `cmake/PerastageDependencies.cmake`, which the root includes before creating
or registering targets that consume the discovered packages.

The root CMake file explicitly registers every application source module with
`add_subdirectory(...)` instead of discovering project sources recursively. The
machine-readable `repository_structure_baseline.json` is the authoritative
module-classification and registration list. The repository-structure guard
keeps that classification, local module CMake ownership, and root registration
from silently diverging. Required architecture guard inventories and stable
documentation markers are validated against the same canonical module list.

The root creates the GUI application target and registers only `main.cpp` and
generated build information. Each source module explicitly owns its production
sources and any focused reusable targets in its local `CMakeLists.txt`; shared
production implementations are linked rather than compiled as private copies.
`cli/` owns the separate `perastage_cli` executable and
`perastage_cli_support` library and contributes no sources to the GUI target.
`tests/` is registered only when testing is enabled.

App lifecycle, Core Command/Query/Inspection services, shared readers, and
frontend dependency rules are owned by
[Architecture](architecture.md#application-bootstrap-ownership), with detailed
contracts linked there. This path map does not duplicate their API inventories.

Target-level platform configuration is dispatched through
`cmake/platform/PerastagePlatform.cmake` to the Windows, macOS, and Linux owners.
Runtime staging belongs to `cmake/PerastageRuntimeStaging.cmake`; installation,
including Linux desktop/MIME/icon integration, belongs to
`cmake/PerastageInstall.cmake`; packaging belongs to
`cmake/PerastagePackaging.cmake`. See [Build](build.md) and
[Packaging](packaging.md) for platform procedures.

## Structural regression guard

[`check_repository_structure_baseline.py`](../../tests/check_repository_structure_baseline.py)
enforces the JSON baseline over Git-tracked files; ignored builds, IDE state,
caches, and untracked artifacts do not affect results. It protects these rules:

1. Vendored C/C++ code belongs under `third_party/`. The guard detects
   vendor-style source directories and explicit upstream-provenance
   metadata outside that owner; ordinary first-party copyright or license
   headers are not treated as evidence of vendoring.
2. Shared build/development configuration cannot acquire developer-specific
   Windows, Linux, macOS, or WSL absolute paths.
3. A top-level directory containing production C/C++ code must be classified by
   the baseline. `tests/` and `third_party/` remain distinct from application
   source modules, while source-free support directories are not misclassified.
4. CMake cannot discover production sources through `file(GLOB ...)` or
   `file(GLOB_RECURSE ...)`; resource globs remain valid.

Tracked developer-local configuration is rejected even if it is force-added:
this includes root `CMakeUserPresets.json`, canonical local build directories,
CMake build-tree metadata, and Visual Studio `.vs/` state. Critical ignore
rules for local presets and the canonical `build/` directory are also part of
the structural contract. `CMakeUserPresets.json` remains the supported,
untracked mechanism for machine-specific preset overrides. Shared build and
development configuration must use portable project or environment variables;
literal developer-home paths are prohibited unless an exact, justified
contract exception is recorded, and stale exceptions fail validation.

Intentional architecture changes should update the declarative baseline,
document the new module's responsibility in the architecture and repository
layout, add appropriate explicit CMake ownership where applicable, and update
the focused fixtures in the same pull request. Source ownership is not
otherwise frozen: intentional future structural changes may move files or
modules when the baseline, documentation, CMake ownership, and focused fixtures
are updated together.

Machine-path exceptions require narrow justification in the baseline; stale
exceptions fail validation. Windows toolchain discovery uses an external
`VCPKG_ROOT` or user-wide integration descriptor, while machine-specific
overrides belong in ignored `CMakeUserPresets.json`. The WSL `/mnt/c` package
isolation paths prevent discovery from crossing into the standard Windows
mount; they do not authorize developer-specific paths below drive mounts.
See [Build](build.md) for supported preset and toolchain configuration.

## Repository-root roles and development entry points

Root files are grouped by structural role in the machine-readable baseline. In
summary, `main.cpp` is the application entry point; `CMakeLists.txt` and
`CMakePresets.json` are build entry points; `setup.sh` and `setup_windows.ps1`
are setup/build launchers; `vcpkg.json` describes dependencies; and `VERSION`
and the license files provide project metadata. `README.md`, the
community documents, and `AGENTS.md` are documentation or repository guidance.
`docs/user/*.md` owns the canonical English user manual; packaged `help/<locale>/`
pages are generated artifacts.

Repository-visible validation starts at `tests/CMakeLists.txt` and
`.github/workflows/ci-tests.yml`; packaging entry points include
`packaging/arch/PKGBUILD` and `packaging/windows/Perastage.iss`.

`CMakeUserPresets.json` is optional, ignored developer-local state for
machine-specific overrides that inherit from the canonical shared presets. It
is not a repository entry point and is not required by setup, CI, or packaging
workflows. See the [build guide](build.md) for the supported preset matrix and a
safe local override example.

### Setup-script ownership and compatibility

The root setup files are stable, discoverable public platform entry points:

- `setup.sh` resolves the checkout from its own location and forwards arguments
  to `scripts/linux/PerastageLinuxBootstrap.sh`. That implementation owns Linux/
  WSL argument parsing, dependency installation, package-manager and command
  checks, preset selection, configure/build sequencing, and diagnostics.
- `setup_windows.ps1` resolves and forwards public PowerShell parameters to
  `scripts/windows/PerastageWindowsBootstrap.ps1`. That implementation owns the
  Visual Studio environment, classic-vcpkg/dependency validation, preset
  selection, cache cleanup, and configure/build sequencing. Its adjacent
  `PerastageWindowsBootstrap.psm1` owns native-process, path, and Git Bash helpers.

Both launchers default to Debug and resolve implementation paths independently
of the caller's working directory. The [build guide](build.md) owns supported
options and invocation examples. Public syntax, accepted options, diagnostics,
exit behavior, and preset mappings are compatibility contracts. Policy tests
and repository documentation use the root commands; CI and packaging use
presets directly.

## Documentation layout

Perastage documentation is intentionally split by audience and responsibility:

| Path | Purpose |
|------|---------|
| `README.md` | Short project overview, highlights, and entry links. |
| [perastage_tree.md](perastage_tree.md) | Concise navigational tree used by architecture guard scripts. |
| `docs/developer/build.md` | Build requirements, dependency setup, and local CMake workflows. |
| `docs/developer/maintainer_runbook.md` | Operational index for routine maintenance, recovery, and external-service dependencies. |
| `docs/developer/packaging.md` | Release packaging, installers, desktop integration, and platform distribution notes. |
| `docs/user/troubleshooting.md` | Known failure modes and practical fixes. |
| `docs/developer/documentation_policy.md` | Documentation organization and synchronization rules. |
| `docs/developer/architecture.md` | Authoritative architecture ownership, module roles, and dependency directions. |
| `docs/developer/repository_layout.md` | Authoritative human-readable repository paths and root-file roles. |
| `docs/developer/repository_structure_baseline.json` | Machine-readable structural contract enforced by tests. |
| `docs/assets/` | Assets used by the documentation website. |
| `docs/*.html` | Static documentation website entry points and shells. |
| `docs/user/` | Canonical English user manual consumed by the website and packaged Help/Console. |
| `docs/developer/` | Build, architecture, packaging, policy, and maintainer guides. |
| `docs/developer/technical-notes/` | Active implementation contracts and maintainer-only technical notes. |
| `docs/reference/` | Intentionally maintained local reference/specification material. |

Avoid duplicating long sections across documentation files. Prefer one source of truth and link to it from related pages.

## Related documents

- [perastage_tree.md](perastage_tree.md)
- [Architecture](architecture.md)
- [Documentation Policy](documentation_policy.md)
- [Build and dependency guide](build.md)
- [Packaging and Platform Integration](packaging.md)
