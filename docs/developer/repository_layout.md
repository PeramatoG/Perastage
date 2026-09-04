# Repository Layout

This page provides a concise map of the main Perastage repository areas. For the broader high-level tree, see [perastage_tree.md](perastage_tree.md). For architectural boundaries and contribution rules, see [Architecture](architecture.md).

The machine-readable ORG-001 baseline is
[`repository_structure_baseline.json`](repository_structure_baseline.json). The
`RepositoryStructureBaseline` policy test validates its required directories,
root-file roles, build/development entry points, and current CMake ownership
model. The JSON file is the authoritative categorized top-level directory list;
this page remains its human-readable architectural counterpart.

## Top-level structure

| Path | Purpose |
|------|---------|
| `main.cpp` | wxWidgets application entry point and top-level initialization. |
| `CMakeLists.txt` | Root build orchestration, global dependencies, platform options, install rules, and module registration. |
| `CMakePresets.json` | Canonical tracked configure/build presets for supported local development workflows. |
| `cmake/` | CMake helper scripts, generated configuration templates, and platform metadata templates. |
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

The root CMake file explicitly registers the main source modules with `add_subdirectory(...)` instead of discovering project sources recursively. The currently registered source modules are:

```text
core/
gui/
viewer2d/
viewer3d/
viewer_common/
```

Keep this list aligned with `CMakeLists.txt` whenever a new top-level source module is introduced.

The current arrangement is intentionally descriptive rather than fully
decentralized. The root `CMakeLists.txt` creates the application target, directly
registers `main.cpp`, generated `build_info.cpp`, and source groups from
`models/`, `mvr/`, and `core/`. The five modules above contribute their explicit
source lists through their own `CMakeLists.txt` files. `tests/` is added
conditionally when testing is enabled. No recursive project-source discovery is
used.

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
   Windows, Linux, macOS, or WSL absolute paths outside the exact transitional
   state recorded in the baseline.
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
the focused fixtures in the same pull request. Source ownership is not otherwise
frozen: later ORG work can move registration into module-owned CMake files while
retaining explicit source lists.

The baseline temporarily records exact file, value, and occurrence counts for
existing `C:/vcpkg` and `C:\vcpkg` references in `CMakeLists.txt`,
`CMakePresets.json`, `CMakeSettings.json`, `CppProperties.json`, and
`setup_windows.ps1`. Each count describes the exact current repository state,
not reusable permission: both additional and missing occurrences fail. ORG-005
through ORG-011 must reduce or remove these entries together with the relevant
legacy configuration; ORG-002 deliberately does not perform that cleanup.

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

`CMakeSettings.json` and `CppProperties.json` are recorded as committed legacy
development configuration. Their future status is deliberately outside this
baseline task. Repository-visible validation starts at `tests/CMakeLists.txt`
and `.github/workflows/ci-tests.yml`; packaging entry points include
`packaging/arch/PKGBUILD` and `packaging/windows/Perastage.iss`.

`CMakeUserPresets.json` is optional, ignored developer-local state for
machine-specific overrides that inherit from the canonical shared presets. It
is not a repository entry point and is not required by setup, CI, or packaging
workflows. See the [build guide](build.md) for the supported preset matrix and a
safe local override example.

## Documentation layout

Perastage documentation is intentionally split by audience and responsibility:

| Path | Purpose |
|------|---------|
| `README.md` | Short project overview, highlights, and entry links. |
| `help.md` | In-app help content. |
| [perastage_tree.md](perastage_tree.md) | High-level repository map used by architecture guard scripts. |
| `docs/developer/build.md` | Build requirements, dependency setup, and local CMake workflows. |
| `docs/developer/packaging.md` | Release packaging, installers, desktop integration, and platform distribution notes. |
| `docs/user/troubleshooting.md` | Known failure modes and practical fixes. |
| `docs/developer/documentation_policy.md` | Documentation organization and synchronization rules. |
| `docs/developer/architecture.md` | Architecture boundaries and project structure conventions. |
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
