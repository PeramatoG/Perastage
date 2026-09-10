# CI and Release Packaging Validation

This document records the ORG-040 validation snapshot for the repository after
the full organization refactor. It deliberately separates static/local checks
from GitHub-hosted build evidence and does not treat workflow inspection as a
successful package build.

## Baseline and audit scope

- ORG-041 reconciliation baseline `main`: `87ee5dd8f516a749fefed853e3cf661ad0a6f360`.
- Reconciliation baseline `VERSION`: `1.6.26`.
- ORG-040 merge: `6631d8d192f544697de3be30015989cc576ce082`.
  ORG-038 merge `056f3f464e65fb0f524dd175964f6fffb55b55df`, ORG-039 merge
  `774d689fb3097ca21af01bbb1fdbd16613e9cd2f`, and the ORG-040 merge are all
  ancestors of this baseline. The baseline is the automatic patch-version
  commit immediately after the ORG-040 merge.
- Original ORG-040 audit baseline `main`: `987ddd461726ee17d7e167c56be04c494dbc5487`.
- Original ORG-040 audit baseline `VERSION`: `1.6.25`.
- ORG-039 merge: `774d689fb3097ca21af01bbb1fdbd16613e9cd2f`, which is an ancestor of the
  original audit baseline. That baseline is the automatic patch-version commit
  immediately after that merge.
- Original audit snapshot: 2026-09-09 UTC; hosted-evidence reconciliation: 2026-09-10 UTC.
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
| Post-merge Debug CI | PASS | Run [#473](https://github.com/PeramatoG/Perastage/actions/runs/34399126771), run id `34399126771`, source `774d689fb3097ca21af01bbb1fdbd16613e9cd2f`: `resolve-source`, `windows-debug`, `linux-debug`, and `macos-debug` all succeeded. |
| Windows installer | PASS | Main Patch Release Artifacts run [#486](https://github.com/PeramatoG/Perastage/actions/runs/34399127104), run id `34399127104`, release source `987ddd461726ee17d7e167c56be04c494dbc5487`: `windows-installer / build-windows-installer` succeeded. |
| Linux AppImage | PASS | Run #486 `linux-installer / build-linux-appimage` succeeded from release source `987ddd461726ee17d7e167c56be04c494dbc5487`; configure, staged build, symbol upload, AppImage build, and AppImage upload all succeeded. Artifacts were `Perastage-linux-appimage`, `Perastage-linux-staged`, and `Perastage-linux-symbols`. |
| Current macOS DMG | PASS | Run #486 `macos-installer / build-macos-installer` succeeded from release source `987ddd461726ee17d7e167c56be04c494dbc5487`. |
| macOS 15 DMG | PASS | Weekly Compatibility Packages run [#9](https://github.com/PeramatoG/Perastage/actions/runs/34446176176), run id `34446176176`, exact source `987ddd461726ee17d7e167c56be04c494dbc5487`, version `1.6.25`: `macos15-installer / build-macos-installer` succeeded. |
| Arch package | PASS | Run #9, run id `34446176176`, exact source `987ddd461726ee17d7e167c56be04c494dbc5487`, version `1.6.25`: `arch-package / build-arch-package` succeeded. |
| Minor-release dry run | NOT INDEPENDENTLY VERIFIED | The public Actions API exposed no post-refactor `Minor Draft Release` run during the ORG-041 reconciliation; its newest visible run was #12 (run id `33170346649`) from 2026-08-28, before the refactor baseline. The reported later successful `dry_run=true` execution therefore cannot be assigned a run id, source SHA, version pair, or job list without fabricating metadata. Static policy tests still confirm that dry run exits before creating a temporary release ref, version commit, tag, package builds, or GitHub Release. |
| Final artifact contract | PASS | Local release tests accepted the exact six-package contract and rejected missing, duplicate, empty, stale, and unexpected assets. Workflow policy tests confirmed that all five builders feed the final validator. |
| Debug-symbol assembly | PASS | Local tests assembled real Windows, Linux, Arch, macOS 15, and current-macOS symbol inputs and rejected missing or malformed inputs. |
| Checksums | PASS | Local final-asset validation generated `SHA256SUMS.txt` only after validating the package set. |
| Provenance | PASS | Local final-asset validation generated and revalidated release provenance, including version and release source identity. |
| Recovery path | PASS | Local publication tests exercised annotated-tag creation and atomic publication against a bare Git remote; static policy checks require recovery to fetch and validate the exact release SHA and validated artifact from the specified run without moving `main`. |
| ORG-040 PR CI | PASS | CI Debug Tests run [#474](https://github.com/PeramatoG/Perastage/actions/runs/34445037552), run id `34445037552`, source head `f1529c9b1d04b3cd6f2ec6a30ea8ce01abfd4eab`: source resolution and all three platform Debug jobs succeeded. |
| Final ORG-040 status | MERGED / COMPLETED | PR #2341 merged as `6631d8d192f544697de3be30015989cc576ce082` after its PR CI passed. Runs #473, #474, #486, and Compatibility Packages #9 supply the independently verifiable hosted CI and package evidence. The separately reported minor-release dry run is not labeled PASS here because its exact hosted metadata was unavailable from the public Actions record. |

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

These checks provide static and local script evidence only; the completed hosted
runs above supply the distinct machine-backed CI and package evidence.

## ORG-041 reconciliation conclusion

The previously pending PR CI, post-merge CI, normal Windows/Linux/current-macOS
packages, and macOS 15/Arch compatibility packages are now successful. This
record does not equate a non-publishing dry run with a real release: no minor
release publication was requested or performed as part of ORG-040 or ORG-041.
The public Actions record did not expose the reported post-refactor dry run, so
its exact metadata remains explicitly unverified rather than inferred.

ORG-040 is merged and complete. ORG-041 is a documentation and checklist
consistency review only and becomes complete on the default branch only when
its own pull request is merged.
