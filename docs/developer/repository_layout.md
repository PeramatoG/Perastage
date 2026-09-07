# Repository Layout

This page is the authoritative human-readable source for Perastage repository
paths, root-file roles, module locations, build/configuration locations, and
repository entry-point ownership. For architectural boundaries and dependency
direction, see [Architecture](architecture.md). For a shorter navigational
view, see [perastage_tree.md](perastage_tree.md).

The machine-readable ORG-001 baseline is
[`repository_structure_baseline.json`](repository_structure_baseline.json). The
`RepositoryStructureBaseline` policy test validates its required directories,
root-file roles, build/development entry points, and current CMake ownership
model. The JSON file is the machine-readable structural contract; this page is
its human-readable layout counterpart.

## Documentation source hierarchy

Repository documentation has one owner for each kind of structural fact:

1. [Architecture](architecture.md) owns architectural responsibilities, module
   roles, and accepted dependency directions.
2. This page owns the human-readable repository path and entry-point map.
3. [`repository_structure_baseline.json`](repository_structure_baseline.json)
   is the machine-readable structural contract enforced by repository checks.
4. [perastage_tree.md](perastage_tree.md) is a concise navigation aid, not a
   second architecture specification.
5. Specialized guides retain procedural detail: [Build](build.md),
   [Localization](localization.md), [Packaging](packaging.md), and
   [GitHub Actions](github_actions_workflows.md). They defer architectural and
   repository ownership rules to the documents above.

## Top-level structure

| Path | Purpose |
|------|---------|
| `main.cpp` | Minimal wxWidgets application-entry wiring; it includes the App-owned declaration and invokes `wxIMPLEMENT_APP(MyApp)`. |
| `CMakeLists.txt` | Project options, principal target creation, shared target configuration, and build-module orchestration. |
| `CMakePresets.json` | Canonical tracked configure/build presets for supported local development workflows. |
| `app/` | wxWidgets application lifecycle and startup/bootstrap composition, explicitly registered by its local CMake file. |
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
from silently diverging.

The source-registration arrangement is decentralized. The root `CMakeLists.txt`
creates the application target and registers only its entry point and generated
build-information source. Every module above contributes its explicit application source
list through its own `CMakeLists.txt`. `tests/` is added
conditionally when testing is enabled. No recursive project-source discovery is
used.

The application-bootstrap boundary in [Architecture](architecture.md#application-bootstrap-ownership-org-030033)
assigns wxWidgets lifecycle and startup orchestration to the top-level `app/`
composition module. Root `main.cpp` remains the sole accepted root C/C++ source
and contains only application-entry wiring. App is registered explicitly and
may depend downward on Core, GUI, and Viewer3D; lower-level modules must not
depend upward on App.

Target-level operating-system configuration is dispatched once through
`cmake/platform/PerastagePlatform.cmake`. The Windows owner configures the
application resource, GUI subsystem, MSVC release symbols, and `Dbghelp`; the
macOS owner configures the application bundle, plist, and icon resources. The
Linux owner documents that no additional target configuration is currently
required, while desktop, MIME, and icon installation remains in
`cmake/PerastageInstall.cmake`.

Cross-cutting viewport interaction preference policy is owned by `core/`, next
to the existing shared selection movement settings. GUI, 2D viewer, and 3D
viewer consumers depend on that policy without introducing viewer or UI
implementation dependencies into Core. The baseline audit treats `main.cpp` as
the only accepted root C/C++ file and rejects any additional root source or
header, case-insensitively by extension, with an actionable diagnostic.

## Structural regression guard

The ORG-002 guard extends the ORG-001 audit rather than creating a parallel
repository checker. It enumerates Git-tracked files, so ignored builds, IDE
state, caches, and other untracked local artifacts do not affect results. It
protects four invariants:

1. Vendored C/C++ code belongs under `third_party/`. The audit conservatively
   detects vendor-style source directories and explicit upstream-provenance
   metadata outside that owner; ordinary first-party copyright or license
   headers are not treated as evidence of vendoring.
2. Shared build/development configuration cannot acquire developer-specific
   Windows, Linux, macOS, or WSL absolute paths.
3. A top-level directory containing production C/C++ code must be classified by
   the baseline. `tests/` and `third_party/` remain distinct from application
   source modules, while source-free support directories are not misclassified.
4. CMake cannot discover production sources through `file(GLOB ...)` or
   `file(GLOB_RECURSE ...)`; resource globs remain valid.

The check uses no branch name or checkout-path assumption and accepts an
explicit tracked-file manifest for isolated fixtures.

Intentional architecture changes should update the declarative baseline,
document the new module's responsibility in the architecture and repository
layout, add appropriate explicit CMake ownership where applicable, and update
the focused fixtures in the same pull request. Source ownership is not
otherwise frozen: intentional future structural changes may move files or
modules when the baseline, documentation, CMake ownership, and focused fixtures
are updated together.

The baseline retains exact-count support for narrowly reviewed machine-path
exceptions, but currently records none. Shared Windows presets load a tracked
bootstrap toolchain that resolves classic vcpkg from a valid external
`VCPKG_ROOT` or the standard user-wide integration descriptor; developer-specific
overrides belong in the ignored `CMakeUserPresets.json`. Fixtures continue to
prove that new machine paths and stale exceptions are rejected.

The `/mnt/c` values in the WSL presets are intentionally portable platform
isolation paths: they prevent Linux package discovery from crossing into the
standard Windows drive mount and are therefore accepted without an exception.
User-specific paths below WSL drive mounts, such as
`/mnt/d/Users/alice/toolchain`, remain forbidden.

## Repository-root roles and development entry points

Root files are grouped by structural role in the machine-readable baseline. In
summary, `main.cpp` is the application entry point; `CMakeLists.txt` and
`CMakePresets.json` are build entry points; `setup.sh` and `setup_windows.ps1`
are setup/build launchers; `vcpkg.json` describes dependencies; and `VERSION`
and the license files provide project metadata. `README.md`, `help.md`, the
community documents, and `AGENTS.md` are documentation or repository guidance.

The obsolete Visual Studio `CMakeSettings.json` and `CppProperties.json` files
were removed after their settings were verified as redundant with the canonical
CMake presets and generated compile database. Repository-visible validation starts at `tests/CMakeLists.txt`
and `.github/workflows/ci-tests.yml`; packaging entry points include
`packaging/arch/PKGBUILD` and `packaging/windows/Perastage.iss`.

`CMakeUserPresets.json` is optional, ignored developer-local state for
machine-specific overrides that inherit from the canonical shared presets. It
is not a repository entry point and is not required by setup, CI, or packaging
workflows. See the [build guide](build.md) for the supported preset matrix and a
safe local override example.

### Setup-script ownership and compatibility

The root setup files remain the discoverable, stable platform entry points.
`setup.sh` is a small Linux/WSL launcher that resolves the checkout from its own
location and delegates unchanged arguments to
`scripts/linux/PerastageLinuxBootstrap.sh`; that implementation owns parsing,
dependency installation, package-manager and command checks, preset selection,
configure/build sequencing, and completion diagnostics. It accepts `Debug` or
`Release` in any argument position plus `--skip-deps`, `--skip-build`, and
`-h`/`--help`; Debug is the default. Help exits successfully, invalid arguments
and setup failures exit nonzero, diagnostics retain their stdout/stderr intent,
and invocation is independent of the caller's working directory. Native Linux
and WSL share apt/dnf handling and the existing Debug
`wsl-x64-debug`/`wsl-debug-build` and Release
`wsl-x64-release`/`wsl-release-build` mappings.

`setup_windows.ps1` is likewise a small Windows launcher. It resolves
`scripts/windows/PerastageWindowsBootstrap.ps1` from its own location and
forwards the public PowerShell parameters. The implementation script owns the
Visual Studio environment, classic-vcpkg and dependency validation, preset
selection, cache cleanup, and configure/build sequencing; the adjacent
`PerastageWindowsBootstrap.psm1` owns reusable native-process, path, and Git
Bash helpers. The public options retain Debug as the default and include
`-Configuration`, `-VcpkgRoot`, `-VisualStudioPath`, `-VisualStudioVersion`,
`-BashExecutable`, `-SkipBuild`, and `-CleanBuild`.
Repository documentation and policy tests refer to the root public commands;
CI and packaging use presets directly rather than invoking either launcher.
Changing the root invocation syntax, accepted options, diagnostics, exit
behavior, or preset mapping would therefore break the supported setup contract.

## Documentation layout

Perastage documentation is intentionally split by audience and responsibility:

| Path | Purpose |
|------|---------|
| `README.md` | Short project overview, highlights, and entry links. |
| `help.md` | In-app help content. |
| [perastage_tree.md](perastage_tree.md) | Concise navigational tree used by architecture guard scripts. |
| `docs/developer/build.md` | Build requirements, dependency setup, and local CMake workflows. |
| `docs/developer/packaging.md` | Release packaging, installers, desktop integration, and platform distribution notes. |
| `docs/user/troubleshooting.md` | Known failure modes and practical fixes. |
| `docs/developer/documentation_policy.md` | Documentation organization and synchronization rules. |
| `docs/developer/architecture.md` | Authoritative architecture ownership, module roles, and dependency directions. |
| `docs/developer/repository_layout.md` | Authoritative human-readable repository paths and root-file roles. |
| `docs/developer/repository_structure_baseline.json` | Machine-readable structural contract enforced by tests. |
| `docs/assets/` | Assets used by the documentation website. |
| `docs/*.html` | Static documentation website entry points and shells. |
| `docs/user/` | User-facing guides shown in the public documentation flow. |
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
