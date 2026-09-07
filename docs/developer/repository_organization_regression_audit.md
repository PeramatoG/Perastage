# ORG-038 Repository-Organization Regression Audit

## Purpose and authority

This record captures the evidence reviewed for ORG-038. It is not an
architecture or repository-layout specification. Module responsibilities and
dependency directions remain authoritative in [Architecture](architecture.md),
human-readable path and entry-point ownership remains authoritative in
[Repository Layout](repository_layout.md), and the enforced machine-readable
structure remains authoritative in
[`repository_structure_baseline.json`](repository_structure_baseline.json).

The audit examined the repository at commit
`b2c6b4e01b8a63483e44c9f941f6efc6940b75fd` on the branch created from `main`
after the merge of PR #2338. The audited history contains the ORG-034
documentation reconciliation (`dd538e1`), the ORG-035--037 enforcement change
(`a65e14e`), its merge (`5af5425`), and the subsequent main-branch version bump
(`b2c6b4e`). The PR #2338 head check suite reported successful Linux Debug,
Windows Debug, and macOS Debug jobs; the preceding main check suite containing
the documentation reconciliation also completed successfully.

Status terms in this record are:

- **PASS**: repository inspection and focused local checks support the contract.
- **FAIL**: a verified structural regression remains unresolved.
- **NOT-EXECUTABLE-IN-LOCAL-ENVIRONMENT**: execution evidence requires a
  platform or dependency not available in the audit environment. This status
  does not convert missing execution evidence into a structural pass.

## Audit results

| Area | Status | Evidence reviewed |
|---|---|---|
| 1. Root and top-level structure | **PASS** | The tracked root, [the baseline](repository_structure_baseline.json), `tests/check_repository_structure_baseline.py`, and its 43 fixture cases agree that `main.cpp` is the sole root C/C++ source; all eight source modules and all support/vendor classifications are present; tracked local configuration and build state are rejected. |
| 2. Source ownership and CMake registration | **PASS** | `app/`, `core/`, `gui/`, `models/`, `mvr/`, `viewer2d/`, `viewer3d/`, and `viewer_common/` each have an explicit local `CMakeLists.txt`; root `CMakeLists.txt` registers each module, conditionally registers `tests/`, and owns only `main.cpp` plus generated `build_info.cpp`. Baseline fixtures reject source globs, missing local ownership, missing root registration, and root-owned feature sources. |
| 3. Focused root CMake ownership | **PASS** | `cmake/PerastageDependencies.cmake`, `PerastageLocalization.cmake`, `PerastageRuntimeStaging.cmake`, `PerastageInstall.cmake`, `PerastagePackaging.cmake`, and `cmake/platform/PerastagePlatform.cmake` retain their focused responsibilities. Their ownership checks and `tests/check_root_cmake_orchestration.py` passed; inspection found no duplicated extracted implementation or reordered root integration. |
| 4. Include ownership and dependency direction | **PASS** | `tests/check_include_directory_ownership.py`, the dependency inventory, and its 11 fixture cases passed. The inventory has no lower-level dependency on `app`; the current `app -> viewer3d` composition edge remains accepted and documented in [Architecture](architecture.md). The source-module inventories in the baseline, direction guard, bootstrap guard, architecture marker, and layout marker agree. |
| 5. Application bootstrap | **PASS** | Root `main.cpp` contains only the App include and `wxIMPLEMENT_APP(MyApp)`. `app/perastage_app.{h,cpp}` owns `MyApp` and the lifecycle paths. `tests/check_application_bootstrap_ownership.py` found no duplicate bootstrap owner and verifies startup, open-file, localization, diagnostics, splash, shutdown, macOS callbacks, local registration, and localization source scanning. `tests/check_startup_window_publication.sh` passed against the App-owned implementation. |
| 6. Setup entry points | **PASS** | The 13-line `setup.sh` delegates all arguments to `scripts/linux/PerastageLinuxBootstrap.sh`; the 25-line `setup_windows.ps1` preserves the public parameters and delegates to `scripts/windows/PerastageWindowsBootstrap.ps1`. Linux and Windows launcher checks passed, and [Build](build.md) and [Repository Layout](repository_layout.md) present the root commands as the public workflow. |
| 7. Canonical local build configuration | **PASS** | `CMakePresets.json`, `.gitignore`, [Build](build.md), and `tests/check_cmake_preset_policy.sh` agree that shared presets are canonical and `CMakeUserPresets.json` is optional, ignored, and untracked. The structure guard found no tracked local build/IDE state or unapproved machine path; fixtures prove those failures and portable WSL/toolchain cases. No ORG-039 clean-checkout workflow was performed. |
| 8. Resource lookup and generated resources | **PASS** (structural) | Root CMake still generates `generated/library/fixtures/Dummy 1ch.gdtf`. `PerastageRuntimeStaging.cmake` stages bundled `library/`, the generated fixture, resources, and generated catalogs into the platform runtime layout; `PerastageInstall.cmake` installs those assets, help/license files, and platform-specific layouts. Localization, runtime-staging, and installation ownership checks passed. Runtime execution across all installed layouts is reserved for later validation. |
| 9. Packaging contract | **PASS** (structural) | `PerastagePackaging.cmake` retains project version/vendor/description metadata, NSIS ownership, uninstall-before-install, `.mvr` registration, optional `.pstg` registration, quoted open commands, and macOS DragNDrop naming. Linux desktop/MIME integration remains in `PerastageInstall.cmake`. `tests/check_packaging_ownership.py` passed. No ORG-040 package generation was performed. |
| 10. Documentation accuracy | **PASS** | [Architecture](architecture.md), [Repository Layout](repository_layout.md), [the concise tree](perastage_tree.md), the baseline, [Build](build.md), [Localization](localization.md), [Packaging](packaging.md), and [GitHub Actions workflows](github_actions_workflows.md) were compared with the tree and focused owners. Documentation links, tree-module alignment, and baseline documentation markers passed. Completed organization work is described as current state; historical audit material remains identifiable as history. |
| 11. Platform support boundaries | **PASS** (structural); **NOT-EXECUTABLE-IN-LOCAL-ENVIRONMENT** (Windows/macOS execution) | The platform dispatcher still selects Windows, macOS, or Linux owners; presets retain Linux/WSL and portable Windows/macOS configuration; launchers, CI jobs, packaging metadata, and resource layouts remain present. `tests/check_platform_cmake_ownership.py` and the setup/preset checks passed. PR #2338 supplied successful Linux, Windows, and macOS Debug CI evidence. This Linux environment cannot execute Windows or macOS builds. |
| 12. Guard coverage | **PASS** | The review covered baseline/root growth, top-level registration and documentation markers, dependency inventories, lower-level App exclusion, source-glob rejection, machine paths, local artifacts, third-party placement, focused CMake owners, include ownership, launchers, bootstrap, localization, staging, installation, packaging, platform dispatch, and CI policy registration. Fixture suites exercise important negative cases. No concrete unenforced ORG-001--037 requirement was found, so no parallel guard was added. |

## Regressions and corrections

No repository-organization regression was found. ORG-038 therefore adds only
this evidence record, its developer-index entry, and the required release-note
entry. It does not alter application sources, resources, build behavior,
packaging behavior, file formats, UI behavior, or architecture guards.

## Local validation evidence

The following focused checks passed in the Linux audit environment:

- Repository baseline and all 43 baseline fixture cases.
- Dependency-direction inventory and all 11 direction fixture cases.
- Application bootstrap, startup publication, include ownership, root CMake
  orchestration, dependency discovery, localization ownership, runtime staging,
  installation, packaging, and platform CMake ownership checks.
- Linux and Windows setup-entry checks, shared-preset policy, CI workflow
  architecture, documentation links, tree-module alignment, GUI configuration
  access policy, localization boundary checks, test include-boundary checks, and
  release-workflow preservation checks.
- Localization catalog `self-test`, `audit`, and `check-po` validation.

A normal `wsl-x64-debug` configure with `BUILD_TESTING=ON` was attempted but is
**NOT-EXECUTABLE-IN-LOCAL-ENVIRONMENT** because wxWidgets development libraries
are not installed. Consequently, the local environment could not build the
native target or run the full CTest suite. The normal PR matrix remains the
authoritative clean Linux/Windows/macOS build and full-CTest evidence.

## Scope boundary and completion evidence

This is structural regression evidence only. It does not perform the ORG-039
clean-checkout local-workflow exercise, ORG-040 release/installer generation,
or ORG-041 checklist editing. ORG-038 completion additionally requires the PR
for this record to report successful Linux Debug, Windows Debug, macOS Debug,
full CTest, and Linux repository-policy jobs; those PR results must be recorded
in the PR discussion/checks rather than rewriting this historical local audit.
