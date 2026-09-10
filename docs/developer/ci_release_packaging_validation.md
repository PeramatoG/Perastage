# CI and Release Packaging Validation

This document records the ORG-040 validation snapshot for the repository after
the full organization refactor. It deliberately separates static/local checks
from GitHub-hosted build evidence and does not treat workflow inspection as a
successful package build.

## Baseline and audit scope

- Baseline `main`: `987ddd461726ee17d7e167c56be04c494dbc5487`.
- Baseline `VERSION`: `1.6.25`.
- ORG-039 merge: `774d689fb3097ca21af01bbb1fdbd16613e9cd2f`, which is an ancestor of the
  baseline. The baseline is the automatic patch-version commit immediately
  after that merge.
- Audit snapshot: 2026-09-09 UTC.
- Workflow and script revision: every path below was audited as stored in
  baseline commit `987ddd461726ee17d7e167c56be04c494dbc5487`.

The audited workflow surface is:

- `.github/workflows/ci-tests.yml`
- `.github/workflows/main-patch-test-build.yml`
- `.github/workflows/windows-installer.yml`
- `.github/workflows/linux-installer.yml`
- `.github/workflows/macos-installer.yml`
- `.github/workflows/compatibility-builds.yml`
- `.github/workflows/macos-15-manual-installer.yml`
- `.github/workflows/arch-package.yml`
- `.github/workflows/minor-draft-release.yml`
- `.github/workflows/recover-minor-release.yml`
- `.github/workflows/vcpkg-binary-cache.yml`
- `.github/release-artifact-contract.json`
- `.github/scripts/assemble_debug_symbols.py`
- `.github/scripts/validate_final_release_assets.py`
- `cmake/PerastageInstall.cmake`, `cmake/PerastagePackaging.cmake`,
  `cmake/PerastageRuntimeAssets.cmake`, and the platform CMake modules.

## Evidence matrix

Only a completed machine-backed check is marked PASS. A hosted package row is
pending unless a successful run built that package from the post-refactor
source.

| Surface | Status | Exact evidence |
|---|---|---|
| PR CI and complete Debug tests | PASS | CI Debug Tests run [#472](https://github.com/PeramatoG/Perastage/actions/runs/34387485173), run id `34387485173`, source `51df89ad5a11bad8a5204faa9b00c112ce703a45`: `resolve-source`, `windows-debug`, `linux-debug`, and `macos-debug` all succeeded. Each platform completed its full build and CTest step and uploaded test results. |
| Post-merge Debug CI | PENDING EXTERNAL VALIDATION | Run [#473](https://github.com/PeramatoG/Perastage/actions/runs/34399126771), run id `34399126771`, source `774d689fb3097ca21af01bbb1fdbd16613e9cd2f`: source resolution, Windows, and Linux succeeded, while macOS and the overall run were still in progress at the audit snapshot. |
| Windows installer | PENDING EXTERNAL VALIDATION | Main Patch Release Artifacts run [#486](https://github.com/PeramatoG/Perastage/actions/runs/34399127104), run id `34399127104`, release source `987ddd461726ee17d7e167c56be04c494dbc5487`: the Windows builder was still building. |
| Linux AppImage | PASS | Run #486 `linux-installer / build-linux-appimage` succeeded from release source `987ddd461726ee17d7e167c56be04c494dbc5487`; configure, staged build, symbol upload, AppImage build, and AppImage upload all succeeded. Artifacts were `Perastage-linux-appimage`, `Perastage-linux-staged`, and `Perastage-linux-symbols`. |
| Current macOS DMG | PENDING EXTERNAL VALIDATION | Run #486 current-macOS builder was still installing dependencies; packaging and DMG validation had not run. |
| macOS 15 DMG | PENDING EXTERNAL VALIDATION | Weekly Compatibility Packages run [#8](https://github.com/PeramatoG/Perastage/actions/runs/34203307608) succeeded, but its source `bfada84e35db41bcd1bc4deb4bd6fbaa24396e76` predates the completed refactor and therefore is supporting evidence only. |
| Arch package | PENDING EXTERNAL VALIDATION | Weekly Compatibility Packages run #8 succeeded, but its source predates the completed refactor and therefore is supporting evidence only. |
| Minor-release dry run | PENDING EXTERNAL VALIDATION | The unauthenticated environment cannot dispatch `minor-draft-release.yml`; no hosted dry run was claimed. Static policy tests confirmed that dry run exits before staging, building, tagging, pushing, or release creation. |
| Final artifact contract | PASS | Local release tests accepted the exact six-package contract and rejected missing, duplicate, empty, stale, and unexpected assets. Workflow policy tests confirmed that all five builders feed the final validator. |
| Debug-symbol assembly | PASS | Local tests assembled real Windows, Linux, Arch, macOS 15, and current-macOS symbol inputs and rejected missing or malformed inputs. |
| Checksums | PASS | Local final-asset validation generated `SHA256SUMS.txt` only after validating the package set. |
| Provenance | PASS | Local final-asset validation generated and revalidated release provenance, including version and release source identity. |
| Recovery path | PASS | Local publication tests exercised annotated-tag creation and atomic publication against a bare Git remote; static policy checks require recovery to fetch and validate the exact release SHA and validated artifact from the specified run without moving `main`. |
| Final ORG-040 status | PENDING EXTERNAL VALIDATION | Run #486 must finish successfully, compatibility packages must be built successfully from an exact post-refactor SHA, the safe minor-release dry run must succeed, and the ORG-040 PR CI must be green. ORG-040 remains unchecked until review and merge. |

## Audit conclusions

The reusable builders checkout `inputs.source_ref`, read `VERSION` from that
checkout, build and package in one job, and upload only after their required
staging/package validation. Main patch packaging passes the immutable SHA of
the version-bump commit to the Windows, Linux, and current-macOS builders.
Compatibility packaging resolves its requested ref once and passes the
resulting SHA to both builders. No required build or package step uses
`continue-on-error`; unconditional steps are limited to diagnostics and
summaries.

The CMake configure and install entry points referenced by the builders use the
current module-owned runtime staging, installation, and packaging rules. The
ownership checks found no dependency on deleted legacy root configuration.
The Debug workflow resolves one source SHA, builds all Debug targets, runs the
complete platform CTest suite, preserves Windows-only registration boundaries,
supports restricted-PATH policy tests, checks tracked-source cleanliness, and
uploads failure diagnostics. Dependency and compiler caches are keyed by
dependency/toolchain compatibility inputs and are not accepted as substitutes
for configure, build, or test success.

The minor release path computes the next `MAJOR.MINOR.0` version, rejects an
existing tag, and makes dry run side-effect free. A real preparation run alone
creates a fully qualified temporary ref. All five platform packages and their
symbols are required before the exact artifact contract is validated; only
then are checksums and provenance produced and publication enabled. Publication
atomically advances `main` and creates the annotated tag, and both the normal
and fallback cleanup paths target only the exact temporary ref. Recovery reads
`VERSION` from the supplied release SHA, revalidates the downloaded final set
and provenance, never advances `main`, and exits before writes in dry-run mode.

## Focused local validation

The following passed against the baseline workflow revision:

- `python3 tests/check_ci_workflow_architecture.py`
- `python3 -m pytest -q tests/ci/test_assemble_debug_symbols.py tests/ci/test_validate_final_release_assets.py tests/ci/test_release_workflow_publication.py tests/ci/check_detached_head_temp_ref.py`
- `python3 tests/check_installation_ownership.py`
- `python3 tests/check_packaging_ownership.py`
- `python3 tests/check_platform_cmake_ownership.py`
- `python3 tests/check_runtime_resource_staging_ownership.py`
- `python3 tests/check_root_cmake_orchestration.py`
- `PERASTAGE_TEST_PYTHON=$(command -v python3) bash tests/check_cmake_preset_policy.sh`
- `PERASTAGE_TEST_PYTHON=$(command -v python3) bash tests/check_release_gate_policy_portability.sh`
- `python3 tests/check_release_workflows_untouched.py`
- `bash tests/check_perastage_tree_modules.sh`
- `bash tests/check_no_configmanager_get_in_gui.sh`
- `python3 tests/check_docs_links.py`

These checks provide static and local script evidence only; they do not replace
the pending hosted package builds.

## Required external validation

1. Confirm run #486 completes successfully with successful Windows, Linux, and
   current-macOS builders and inspect the uploaded installer/AppImage/DMG names.
2. Dispatch `compatibility-builds.yml` with
   `source_ref=987ddd461726ee17d7e167c56be04c494dbc5487` (or the reviewed ORG-040 head)
   and require source resolution, macOS 15, Arch, and summary jobs to succeed.
3. Dispatch `minor-draft-release.yml` from the reviewed ORG-040 branch with
   `dry_run=true`; confirm that metadata/contract validation succeeds and that
   no temporary ref, version commit, tag, build, or release is created.
4. Require all jobs in the ORG-040 pull request's CI Debug Tests workflow to
   succeed before merge.
5. After review and successful external evidence, merge the ORG-040 PR. Only a
   later main-branch change may mark the immutable ORG-040 checklist complete.

No ORG-041 work is included in this validation.
