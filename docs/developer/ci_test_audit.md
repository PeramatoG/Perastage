# CI Debug Tests audit

Baseline SHA: `ec6dee42371f51dc02520e3eb9fc4bea4d0daeca`.
Audit date: 2026-07-22.

## Standards references pinned for this audit

The audit uses the current upstream public documentation for the project policy target versions:

- GDTF: DIN SPEC 15800:2022-02, GDTF Version 1.2, documented by GDTF Hub at `https://www.gdtf.eu/gdtf/` and `https://www.gdtf.eu/gdtf/prologue/introduction/` on 2026-07-22.
- MVR: DIN SPEC 15801:2023-12, MVR Version 1.6, documented by GDTF Hub at `https://www.gdtf.eu/mvr/` and `https://www.gdtf.eu/mvr/prologue/introduction/` on 2026-07-22.

Normal PR tests must use local deterministic fixtures and must not download live GDTF Share or schema content during execution.

## Baseline inventory attempt

The local Linux inventory was attempted with:

```sh
cmake -S . -B out/ci-audit-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DPERASTAGE_ENABLE_COMPILER_CACHE=OFF
ctest --test-dir out/ci-audit-debug -N -V > out/ci-audit-ctest-inventory.txt
```

The configure step failed before test generation because this container does not provide wxWidgets development files. No source-tree test artifacts were generated. The failed configure output is an environment limitation, not a test result, and the intended generated inventory location remains `out/ci-audit-ctest-inventory.txt`.

## Current classification matrix

| Test or group | Primary classification | Affected platform | First meaningful failure | Contract | Remediation in this change |
| --- | --- | --- | --- | --- | --- |
| `ReleaseGatePolicyPortability` | `test-harness-defect` | macOS, Windows Git Bash, Linux restricted-path simulation | The test hard-coded `/usr/bin/bash`, `/usr/bin/dirname`, `/usr/bin/env`, `/tmp`, and symbolic links. | Release-gate policy scripts must run from the repository root and unrelated working directories using only declared tools, and must not require ripgrep. | Reworked the portability harness to use the CMake-provided or current Bash executable, discover required tools with `command -v`, use a platform-selected temporary root, copy helper tools instead of symlinking them, and execute the scripts under a restricted PATH without `rg`. |
| Portable shell tests invoking Python | `test-harness-defect` | Windows primarily; all platforms as policy | Historical failures could reach unresolved Python aliases such as the Microsoft Store launcher. | Portable tests must use the CMake-resolved Python interpreter through `PERASTAGE_TEST_PYTHON`, `resolve_test_python`, or `run_test_python`. | Added `UnresolvedPythonInvocations`, a repository-policy test that scans portable test files for direct unresolved `python` or `python3` command invocations. |
| Full Linux CTest failure groups from prior artifact | `standards-ambiguity-needing-documentation` | Linux baseline artifact | Prior artifact reported MVR/GDTF, rider, layout, PDF, path, geometry, and GUI failures. | Each failure must be reproduced on the latest main SHA before assertions or production behavior are changed. | Not modified in this focused harness repair. The local container cannot configure the suite because wxWidgets development files are unavailable, so these remain pending for an environment with the project dependencies installed. |
| macOS release-gate-only coverage | `platform-specific-contract` | macOS CI | macOS Debug CI currently builds and runs only release-gate coverage. | CI profiles must make platform coverage explicit and not silently treat a narrow gate as full-suite parity. | Documented as pending audit work; no workflow coverage reduction was made. |

## Decisions

- The release-gate portability failure is a harness defect. It did not expose a production-code defect because the failing assumptions were in the policy test's own runner setup.
- The Python alias risk is a harness policy defect. The repaired policy prevents new portable tests from bypassing the resolved interpreter.
- No MVR/GDTF, rider, layout, PDF, 3DS, path, or GUI assertion was changed in this step because the failures were not reproducible in this container after the required configure failure.
- No obsolete test was removed or quarantined.

## Remaining audit work

1. Re-run configure, build, `ctest -N -V`, and the full Debug CTest suite on Linux, Windows, and macOS runners with project dependencies installed.
2. Export the complete machine-readable test inventory under `out/` with CTest name, source, target, platform availability, policy layer, domain labels, scope, fixture category, timeout, and required services/tools.
3. Classify every current failing test using the project classification taxonomy before changing assertions.
4. Split `tests/CMakeLists.txt` into domain modules only after current behavior and root causes are understood.
5. Implement functional PR and full/release CI profiles without hiding known failures.

## Completed baseline from CI run 29961799720

Branch base SHA: `ec6dee42371f51dc02520e3eb9fc4bea4d0daeca`.
Reviewed head for run `29961799720`: `4732460bc10bd2e312bb2714d8a6e82f617c5a01`.
Current follow-up head: recorded in PR #2204 after the final commit for this task.
Workflow: `CI Debug Tests`.
Run: `29961799720` (`https://github.com/PeramatoG/Perastage/actions/runs/29961799720`).

All three platform jobs completed dependency installation, CMake configuration, and their requested builds. Linux and Windows then failed during CTest execution. macOS passed its reduced release-gate CTest profile. The macOS job is not a full-suite platform-parity run.

| Platform | Configure result | Build result | CTest result | Total executed | Passed | Failed | Skipped | Not run | Artifact/log reference |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| Linux | Passed | Passed | Failed | 158 | 132 | 25 | 1 | 0 | `ci-linux-debug-diagnostics` from run `29961799720`; `out/ci-logs/ctest-linux-debug.log`; `out/ci-logs/ctest-linux-debug.junit.xml`; `out/ci-logs/ctest-linux-debug-failures.csv`; `build/linux-debug/Testing/Temporary/LastTestsFailed.log` |
| Windows | Passed | Passed | Failed | 158 | 128 | 30 | 0 | 0 | `ci-windows-debug-diagnostics` from run `29961799720`; `out/ci-logs/ctest-windows-debug.log`; `out/ci-logs/ctest-windows-debug.junit.xml`; `out/ci-logs/ctest-windows-debug-failures.csv`; `build-windows-debug/Testing/Temporary/LastTestsFailed.log` |
| macOS | Passed | Passed two requested release-gate executables | Passed reduced `release-gate` label profile | 6 | 6 | 0 | 0 | Full suite not run | Successful run `29961799720`; prior workflow uploaded detailed artifacts only on failure, so no JUnit/result artifact was retained for the passing macOS job |

### Harness checks verified by run 29961799720

- Linux `ReleaseGatePolicyPortability` passed in approximately 5.6 seconds.
- Linux `PythonResolvedInterpreterPolicy` passed.
- Linux `UnresolvedPythonInvocations` passed.
- Windows `PythonResolvedInterpreterPolicy` passed, confirming the resolved interpreter was used instead of the Microsoft Store launcher.
- Windows `UnresolvedPythonInvocations` passed.
- macOS six release-gate tests passed.

### Confirmed harness defect from run 29961799720

| Test | Platform | First meaningful failure line | Evidence | Remediation |
| --- | --- | --- | --- | --- |
| `ReleaseGatePolicyPortability` | Windows Git Bash | `ReleaseGatePolicyPortability ...***Timeout 120.00 sec` after completing `check_windows_ninja_x64_policy.sh` and before `check_ci_cmake_language_policy.sh` completed | `ci-windows-debug-diagnostics`, `out/ci-logs/ctest-windows-debug.log`, `build-windows-debug/Testing/Temporary/LastTestsFailed.log` | The likely slow path was verified locally as the CMake language policy's unpruned `Path('.').rglob('CMakeLists.txt')` traversal. The policy now prunes generated/vendor directory names before descent and the portability harness prints low-noise per-child timings. |

### Common Linux and Windows unclassified domain failures

These failures are current baseline failures only. They are intentionally not classified as production defects, stale expectations, or invalid fixtures until the next domain-specific audit phase traces each contract.

| Test | First meaningful failure line | Reproduction command | Artifact/log reference |
| --- | --- | --- | --- |
| `Viewer2DFboCaptureDiagnostics` | `Viewer2DFboCaptureDiagnostics ...***Failed` | `ctest --test-dir <build-dir> -R '^Viewer2DFboCaptureDiagnostics$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `EditableFocusUtils` | `EditableFocusUtils ...***Failed`; Linux output also included an AT-SPI/DBus warning before the failure | `ctest --test-dir <build-dir> -R '^EditableFocusUtils$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `GdtfReadServices` | `GdtfReadServices ...***Failed` | `ctest --test-dir <build-dir> -R '^GdtfReadServices$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `GdtfFixtureCategoryFallback` | `GdtfFixtureCategoryFallback ...***Failed` | `ctest --test-dir <build-dir> -R '^GdtfFixtureCategoryFallback$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `LayoutTemplatePackageService` | `LayoutTemplatePackageService ...***Failed` | `ctest --test-dir <build-dir> -R '^LayoutTemplatePackageService$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `PdfTextComparison` | `PdfTextComparison ...***Failed` | `ctest --test-dir <build-dir> -R '^PdfTextComparison$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `SaveLoadRoundtrip` | `SaveLoadRoundtrip ...***Failed` | `ctest --test-dir <build-dir> -R '^SaveLoadRoundtrip$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `ProjectSupportUserDataRoundtrip` | `ProjectSupportUserDataRoundtrip ...***Failed` | `ctest --test-dir <build-dir> -R '^ProjectSupportUserDataRoundtrip$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `TrussPathEncodingRegression` | `TrussPathEncodingRegression ...***Failed` | `ctest --test-dir <build-dir> -R '^TrussPathEncodingRegression$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `RiderTrussDictionaryNormalization` | `RiderTrussDictionaryNormalization ...***Failed` | `ctest --test-dir <build-dir> -R '^RiderTrussDictionaryNormalization$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `MvrSupportUserDataRoundtrip` | `MvrSupportUserDataRoundtrip ...***Failed` | `ctest --test-dir <build-dir> -R '^MvrSupportUserDataRoundtrip$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `MvrFixtureCategoryRoundtrip` | `MvrFixtureCategoryRoundtrip ...***Failed` | `ctest --test-dir <build-dir> -R '^MvrFixtureCategoryRoundtrip$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `MvrTrussRoundtripStructure` | `MvrTrussRoundtripStructure ...***Failed` | `ctest --test-dir <build-dir> -R '^MvrTrussRoundtripStructure$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `MvrExporterCompliance` | `MvrExporterCompliance ...***Failed` | `ctest --test-dir <build-dir> -R '^MvrExporterCompliance$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `RiderImportLinearOrder` | `RiderImportLinearOrder ...***Failed` | `ctest --test-dir <build-dir> -R '^RiderImportLinearOrder$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `RiderFilterPreview` | `RiderFilterPreview ...***Failed` | `ctest --test-dir <build-dir> -R '^RiderFilterPreview$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `RiderComments` | `RiderComments ...***Failed` | `ctest --test-dir <build-dir> -R '^RiderComments$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `RiderLedScreenObject` | `RiderLedScreenObject ...***Failed` | `ctest --test-dir <build-dir> -R '^RiderLedScreenObject$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `RiderHoistImport` | `RiderHoistImport ...***Failed` | `ctest --test-dir <build-dir> -R '^RiderHoistImport$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `RiderLxSidesImport` | `RiderLxSidesImport ...***Failed` | `ctest --test-dir <build-dir> -R '^RiderLxSidesImport$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `RiderPipeImport` | `RiderPipeImport ...***Failed` | `ctest --test-dir <build-dir> -R '^RiderPipeImport$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `Loader3dsNativeDimensions` | `Loader3dsNativeDimensions ...***Failed` | `ctest --test-dir <build-dir> -R '^Loader3dsNativeDimensions$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `GdtfLoaderSetPropertiesMutation` | `GdtfLoaderSetPropertiesMutation ...***Failed` | `ctest --test-dir <build-dir> -R '^GdtfLoaderSetPropertiesMutation$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `SymbolFixtureApplierGdtfMutation` | `SymbolFixtureApplierGdtfMutation ...***Failed` | `ctest --test-dir <build-dir> -R '^SymbolFixtureApplierGdtfMutation$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |
| `MvrPatchedGdtfExportMutation` | `MvrPatchedGdtfExportMutation ...***Failed` | `ctest --test-dir <build-dir> -R '^MvrPatchedGdtfExportMutation$' --output-on-failure --verbose` | Linux/Windows CTest log and failure CSV from run `29961799720` |

### Additional Windows-only failures

| Test | First meaningful failure line | Reproduction command | Artifact/log reference |
| --- | --- | --- | --- |
| `ReleaseGatePolicyPortability` | `ReleaseGatePolicyPortability ...***Timeout 120.00 sec` | `ctest --test-dir build-windows-debug -R '^ReleaseGatePolicyPortability$' --output-on-failure --verbose --interactive-debug-mode 0 --timeout 120` | `ci-windows-debug-diagnostics` from run `29961799720` |
| `PdfWriterSerialization` | `PdfWriterSerialization ...***Failed` | `ctest --test-dir build-windows-debug -R '^PdfWriterSerialization$' --output-on-failure --verbose --interactive-debug-mode 0 --timeout 120` | `ci-windows-debug-diagnostics` from run `29961799720` |
| `GdtfFixtureInsertionPreparation` | `GdtfFixtureInsertionPreparation ...***Failed` | `ctest --test-dir build-windows-debug -R '^GdtfFixtureInsertionPreparation$' --output-on-failure --verbose --interactive-debug-mode 0 --timeout 120` | `ci-windows-debug-diagnostics` from run `29961799720` |
| `LayoutImageResourceRegistry` | `LayoutImageResourceRegistry ...***Failed` | `ctest --test-dir build-windows-debug -R '^LayoutImageResourceRegistry$' --output-on-failure --verbose --interactive-debug-mode 0 --timeout 120` | `ci-windows-debug-diagnostics` from run `29961799720` |
| `GdtfShareSecurity` | `GdtfShareSecurity ...***Failed` | `ctest --test-dir build-windows-debug -R '^GdtfShareSecurity$' --output-on-failure --verbose --interactive-debug-mode 0 --timeout 120` | `ci-windows-debug-diagnostics` from run `29961799720` |

### EditableFocusUtils Phase 2 note

`EditableFocusUtils` remains an unclassified GUI/headless failure. Linux output includes an AT-SPI/DBus warning, but the available baseline does not prove whether that warning is causal or merely environmental noise before a separate wx lifecycle or focus assertion failure. Windows reports the test as failed without a differentiated first assertion in the baseline summary. The focused reproduction command is `ctest --test-dir <build-dir> -R '^EditableFocusUtils$' --output-on-failure --verbose`. No stderr suppression, skip, assertion change, or product-code change was made in this task.

### Result and inventory artifact contract after this follow-up

Each platform now writes both human-readable and machine-readable inventory/results under `out/ci-logs`:

- `ctest-inventory-<platform>-debug.txt` from `ctest -N -V`.
- `ctest-inventory-<platform>-debug.json` from `ctest --show-only=json-v1`.
- `ctest-<platform>-debug.junit.xml`.
- `ctest-<platform>-debug.log` and, for macOS, the wrapper/full CTest logs.
- `ctest-<platform>-debug-failures.csv`.
- `ctest-<platform>-debug-results.json` containing total, passed, failed, skipped, disabled/not-run, selected labels/profile, and tested SHA.
- `LastTestsFailed.log` and `LastTestsDisabled.log` when CTest produces them.

The small `ci-<platform>-debug-test-results` artifacts are uploaded with `if: always()`. The existing heavy `ci-<platform>-debug-diagnostics` artifacts remain failure-only. Result collection preserves CTest exit codes and does not hide failing suites.

### Safe merge status

Safe Merge Point A is reached for the local harness repair and baseline documentation. Safe Merge Point B requires the next CI run for this follow-up commit to confirm that `ReleaseGatePolicyPortability` passes on Windows Git Bash as well as Linux/macOS and that all per-platform result artifacts are present.

## Phase 2 follow-up for CI run 29988207383

Base SHA: `ec6dee42371f51dc02520e3eb9fc4bea4d0daeca`.
Reviewed head for run `29988207383`: `6575fc25792f45264a0092fcd69994a87b87da59`.
Current follow-up head: recorded in PR #2204 after the final commit for this task.
Workflow run: `29988207383` (`https://github.com/PeramatoG/Perastage/actions/runs/29988207383`).

All jobs completed dependency installation and CMake configuration. Linux and Windows built the complete test target set. macOS remained intentionally reduced to the release-gate profile and must not be described as full macOS platform parity.

| Platform | Configure result | Build result | CTest result | Inventory total | Passed | Failed | Skipped | Disabled/not run | Selected labels/profile | Result artifact |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| Linux | Passed | Passed complete test target set | Failed | 159 | 133 | 25 | 1 | 0 | Full Debug suite | `ci-linux-debug-test-results` |
| Windows | Passed | Passed complete MSVC Hostx64/x64 Ninja test target set | Failed | 159 | 129 | 30 | 0 | 0 | Full Debug suite | `ci-windows-debug-test-results` |
| macOS | Passed | Passed requested release-gate executables | Passed | Reduced profile only | 6 | 0 | 0 | 0 | `release-gate` | `ci-macos-debug-test-results` |

### Confirmed remaining harness root cause

The remaining Windows harness failure in run `29988207383` was `ReleaseGatePolicyPortability`. The focused timing evidence showed `WindowsNinjaX64Policy` passed as an independent CTest in approximately 94.74 seconds. Inside `ReleaseGatePolicyPortability`, `check_windows_ninja_x64_policy.sh` completed its repository-root execution in approximately 90 seconds, then the wrapper timed out at the existing 120-second CTest timeout before the unrelated-working-directory execution completed. `check_ci_cmake_language_policy.sh` completed in under one second, proving it was no longer the bottleneck.

The root cause is the Windows policy scanner's unpruned `Path.rglob('*')` traversal. It filtered `.git`, `.vcpkg-cache`, `vcpkg`, `build`, `out`, and similar roots only after descending through them. On Windows CI, the checkout contains large generated/cache/dependency trees, so the scanner spent most of its time traversing files that were never part of the first-party policy contract.

### Remediation in this follow-up

`check_windows_ninja_x64_policy.sh` now uses a deterministic top-down `os.walk` traversal and mutates `dirnames` before descent to prune `.git`, `.vcpkg-cache`, `.vcpkg-root`, `.tools`, `build`, `build-*`, `cmake-build-*`, `out`, `third_party`, `vcpkg`, and `vcpkg_installed`. It still scans first-party files for forbidden generated local Windows preset references such as `local Windows preset marker`, reports a concise inspected-file count and elapsed scan time, uses `PERASTAGE_POLICY_ROOT` only for deterministic fixture testing, and continues to use the resolved Python interpreter supplied by the harness.

A new `WindowsNinjaX64PolicyFixtures` policy fixture proves that forbidden `local Windows preset marker` text in excluded generated/vendor/cache roots is ignored, a first-party nested violation is still detected, and the scanner emits an observable first-party inspected-file count. No broad first-party exclusions, timeout increase, skip, `continue-on-error`, or exit-code masking were introduced.

### Current failure sets preserved for Phase 3

The 25 common Linux/Windows domain failures remain untouched and unclassified beyond baseline preservation:

- `Viewer2DFboCaptureDiagnostics`
- `EditableFocusUtils`
- `GdtfReadServices`
- `GdtfFixtureCategoryFallback`
- `LayoutTemplatePackageService`
- `PdfTextComparison`
- `SaveLoadRoundtrip`
- `ProjectSupportUserDataRoundtrip`
- `TrussPathEncodingRegression`
- `RiderTrussDictionaryNormalization`
- `MvrSupportUserDataRoundtrip`
- `MvrFixtureCategoryRoundtrip`
- `MvrTrussRoundtripStructure`
- `MvrExporterCompliance`
- `RiderImportLinearOrder`
- `RiderFilterPreview`
- `RiderComments`
- `RiderLedScreenObject`
- `RiderHoistImport`
- `RiderLxSidesImport`
- `RiderPipeImport`
- `Loader3dsNativeDimensions`
- `GdtfLoaderSetPropertiesMutation`
- `SymbolFixtureApplierGdtfMutation`
- `MvrPatchedGdtfExportMutation`

The Windows-only domain failures remain untouched and unclassified:

- `PdfWriterSerialization`
- `GdtfFixtureInsertionPreparation`
- `LayoutImageResourceRegistry`
- `GdtfShareSecurity`

`ReleaseGatePolicyPortability` is the only Windows-only failure addressed in this task, and it was addressed as a harness traversal defect.

### Future MVR/GDTF I/O policy for Phase 3 branches

- Be permissive on input: Perastage should recover safely and deterministically from damaged, incomplete, legacy, or non-conforming MVR/GDTF files whenever possible, preserve useful data, and emit structured diagnostics instead of rejecting recoverable files unnecessarily.
- Be canonical on output: every MVR or GDTF archive generated or rewritten by Perastage must conform to the current supported standards target, currently GDTF 1.2 and MVR 1.6, normalize recoverable input, and must not reproduce malformed source structures.
- Keep standard-strict, legacy-compatibility, tolerant-recovery, and Perastage-extension code paths and tests explicitly separated.
- Never make the reader less compatible merely to make a strict writer test pass.

### Safe Merge Point B decision

Safe Merge Point B is pending until a completed CI run for the follow-up commit confirms that `ReleaseGatePolicyPortability` passes on Windows Git Bash within the unchanged 120-second timeout and that the domain failure set is unchanged except for removing the harness timeout. Safe Merge Point B must not be declared from local Linux-only evidence.

## Authoritative current baseline and Phase 3 classification for CI run 29995392074

Base branch verification for this Phase 3 task: the local task branch started at `dea118f7bb2dcdf04e0c415c891b4aa0c305153c`, matching the requested current `main` reviewed for this task. The local checkout has no `origin` remote configured, so `git fetch origin main` could not be completed in this container; the commit graph still shows `dea118f7` immediately after merge commit `381fb1ae` and tested commit `cf10f50`. The only file changed from `cf10f50184376d32c0806d9b5b6de9d186637449` to `dea118f7bb2dcdf04e0c415c891b4aa0c305153c` is `VERSION`, bumped from `1.4.148` to `1.4.149`, so run `29995392074` remains the authoritative baseline for current code and tests.

Run `29995392074` (`https://github.com/PeramatoG/Perastage/actions/runs/29995392074`) is the current authoritative Phase 2 closure baseline. PR #2204 was merged by merge commit `381fb1ae7dcac1936c39072615fc097ef5c51e6b`. Safe Merge Point B is reached. Safe Merge Point C is not declared yet. This follow-up keeps the classification record reviewable while noting entries whose exact artifact diffs, Windows exits, or source-level call paths still need focused Phase 4 evidence before a safe merge decision can be made.

| Platform | Inventory | Executed profile | Passed | Failed | Skipped | Notes |
| --- | ---: | --- | ---: | ---: | ---: | --- |
| Linux Debug | 160 tests | Full Debug suite | 134 | 25 | 1 | `CredentialStoreNativeRoundTrip` is the single skip and is not counted as a failure. |
| Windows Debug | 160 tests | Full Debug suite | 131 | 29 | 0 | `WindowsNinjaX64Policy` passed in about 0.76 seconds; `ReleaseGatePolicyPortability` passed in about 2.92 seconds. |
| macOS Debug | 160 registered tests | Reduced `release-gate` profile | 6 | 0 | 0 | This is not full-suite macOS parity; only 6 release-gate tests executed. |

Always-on result artifacts inspected/recorded for run `29995392074`: `ci-linux-debug-test-results`, `ci-windows-debug-test-results`, and `ci-macos-debug-test-results`. Heavy diagnostics reserved for missing context: `ci-linux-debug-diagnostics` and `ci-windows-debug-diagnostics`. Each result artifact is expected to contain `ctest-inventory-<platform>-debug.json`, `ctest-inventory-<platform>-debug.txt`, `ctest-<platform>-debug.junit.xml`, `ctest-<platform>-debug.log`, `ctest-<platform>-debug-failures.csv`, `ctest-<platform>-debug-results.json`, and `LastTestsFailed.log`/`LastTestsDisabled.log` when CTest produced them. Public GitHub run metadata lists all five artifact names, but this container does not have `gh`, so downloaded archives are not committed and artifact access is recorded by run/artifact name.

`ReleaseGatePolicyPortability` is recorded separately as fixed: primary classification `test-harness-defect`, fixed by PR #2204, absent from the current Windows failure set, and verified passing on Linux, Windows, and macOS in run `29995392074`. It is not counted among the 29 current domain failures.

Current validation follow-up: run `30010572238` is retained as a blocking Phase 2/3 infrastructure failure because macOS failed during configure before CTest inventory, build, or tests when non-Windows Bash discovery was blocked by an empty `find_program` result variable. This resolver defect is addressed by the follow-up bootstrap commit and must be verified by a later completed CI run before Safe Merge Point C can be declared.


### Phase 3 classification entries

#### `GdtfReadServices`

- Exact CTest name: `GdtfReadServices`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux: `gdtf_read_services_test.cpp:492 ... Assertion '!read.Success()' failed.` Windows: `Assertion failed: read.Success(), ... gdtf_read_services_test.cpp, line 402`.
- Source test file and assertion or exit point: `tests/gdtf_read_services_test.cpp:402,492`.
- Production components and call path under test: gdtf read services -> wxZipInputStream/std::filesystem path conversion -> structured diagnostics.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^GdtfReadServices$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `recovery`.
- Policy layer: `tolerant-recovery`.
- Intended current contract: GDTF archive input must be permissive and diagnosable; Unicode ZIP names should be accepted when decoded safely.
- Relevant GDTF/MVR requirement or internal Perastage contract: GDTF archive ZIP with description.xml; internal tolerant input policy.
- Shared root-cause group: GDTF Unicode/archive tolerant-read divergence.
- Confidence: `provisional`.
- Evidence supporting the category: Run 29995392074 shows a platform split: Linux reaches the Unicode ZIP filename subtest and the tolerant read succeeds despite the old negative expectation, while Windows fails an earlier Unicode/inaccessible path read. The entry therefore records a Linux stale-expectation hypothesis and a separate Windows path-access/recovery hypothesis under one CTest name.
- Evidence still missing: Full downloaded JUnit/log segments and the canonicalizer/path diagnostics must be attached before assigning one common root cause.
- Recommended follow-up phase and action: Phase 4A: split Windows path-access evidence from Linux tolerant-read expectation before changing reader behavior or assertions.

#### `GdtfFixtureCategoryFallback`

- Exact CTest name: `GdtfFixtureCategoryFallback`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion ... inferred.category == testCase.expected ... line 177`.
- Source test file and assertion or exit point: `tests/gdtf_fixture_category_test.cpp:177`.
- Production components and call path under test: fixture category parser -> GDTF metadata/category inference -> dictionary fallback.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^GdtfFixtureCategoryFallback$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `generated`.
- Policy layer: `tolerant-recovery`.
- Intended current contract: Missing category metadata should use documented fallback without inventing unsupported standard data.
- Relevant GDTF/MVR requirement or internal Perastage contract: GDTF fixture metadata is optional; Perastage fallback is an internal tolerant-recovery contract.
- Shared root-cause group: Category propagation and fallback contract.
- Confidence: `medium`.
- Evidence supporting the category: Test cases generate minimal GDTF XML and assert inferred.category.
- Evidence still missing: Exact expected/actual from JUnit unavailable in local checkout.
- Recommended follow-up phase and action: Phase 4B: document category fallback precedence then update behavior or expectation.

#### `GdtfFixtureInsertionPreparation`

- Exact CTest name: `GdtfFixtureInsertionPreparation`.
- Affected platform or platforms: Windows.
- First meaningful failure per affected platform: Windows: `error reading zip local header`; `Assertion failed: preparation.success ... line 211`, followed by CRT leak output.
- Source test file and assertion or exit point: `tests/gdtf_fixture_insertion_preparation_test.cpp:61`.
- Production components and call path under test: GUI insertion preparation service -> GDTF metadata read -> diagnostics collection.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^GdtfFixtureInsertionPreparation$' --output-on-failure --verbose`.
- Primary classification: `platform-specific-contract`.
- Fixture category: `generated`.
- Policy layer: `platform-specific`.
- Intended current contract: Insertion preparation must be path-portable and report recoverable diagnostics without rejecting accessible fixtures.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal Windows Unicode/path contract for fixture insertion.
- Shared root-cause group: GDTF Windows path/archive handling.
- Confidence: `medium`.
- Evidence supporting the category: Failure is Windows-only; source helper checks diagnostic presence.
- Evidence still missing: Exact failing diagnostic from Windows artifact not available locally.
- Recommended follow-up phase and action: Phase 4A: reproduce on Windows and decide path conversion fix vs expectation.

#### `GdtfLoaderSetPropertiesMutation`

- Exact CTest name: `GdtfLoaderSetPropertiesMutation`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion SetGdtfProperties(gdtfPath, 12.345f, 678.9f, "Perastage Tests") ... line 69`. The failure occurs at the mutation call before a later output assertion.
- Source test file and assertion or exit point: `tests/gdtfloader_set_properties_test.cpp`.
- Production components and call path under test: GDTF loader/editor mutation -> canonicalizer -> archive writer.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^GdtfLoaderSetPropertiesMutation$' --output-on-failure --verbose`.
- Primary classification: `production-defect`.
- Fixture category: `generated`.
- Policy layer: `standard-strict`.
- Intended current contract: Mutated GDTF output must be canonical GDTF 1.2 and preserve intended edited properties.
- Relevant GDTF/MVR requirement or internal Perastage contract: GDTF 1.2 output archive must contain canonical description.xml and valid fixture metadata.
- Shared root-cause group: GDTF mutation/canonical publication.
- Confidence: `medium`.
- Evidence supporting the category: The observed failing stage is the set-properties mutation call itself; canonicalizer/writer diagnostics are the next trace point, not yet a proven exact defect.
- Evidence still missing: Exact `SetGdtfProperties(...)` diagnostic path and generated XML/archive diff are still missing.
- Recommended follow-up phase and action: Phase 4A: inspect generated archive and normalize writer or assertions per strict output contract.

#### `SymbolFixtureApplierGdtfMutation`

- Exact CTest name: `SymbolFixtureApplierGdtfMutation`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux: `Assertion symbol_preview::ApplySymbolsToFixtureGdtf(...) ... line 162`. Windows: same assertion at line 163.
- Source test file and assertion or exit point: `tests/symbol_fixture_applier_gdtf_test.cpp`.
- Production components and call path under test: symbol fixture applier -> GDTF apply adapter -> archive mutation/writer.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^SymbolFixtureApplierGdtfMutation$' --output-on-failure --verbose`.
- Primary classification: `production-defect`.
- Fixture category: `generated`.
- Policy layer: `standard-strict`.
- Intended current contract: Applying symbols must produce canonical GDTF output while preserving unrelated valid content.
- Relevant GDTF/MVR requirement or internal Perastage contract: GDTF output canonicalization and Perastage symbol mutation contract.
- Shared root-cause group: GDTF mutation/canonical publication.
- Confidence: `medium`.
- Evidence supporting the category: Shares cross-platform mutation failure pattern with loader and patched MVR export tests.
- Evidence still missing: Exact generated symbol resource diff missing.
- Recommended follow-up phase and action: Phase 4A: fix mutation pipeline once for direct and MVR-embedded GDTF.

#### `MvrPatchedGdtfExportMutation`

- Exact CTest name: `MvrPatchedGdtfExportMutation`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `MVR export failed during CanonicalizeGdtf entry='Perastage_QA@Truss_QA_Model@Perastage.gdtf' ... canonicalizer reported errors`; final assertion at line 125.
- Source test file and assertion or exit point: `tests/mvr_patched_gdtf_export_test.cpp`.
- Production components and call path under test: MVR exporter -> patched embedded GDTF writer -> archive validation.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^MvrPatchedGdtfExportMutation$' --output-on-failure --verbose`.
- Primary classification: `production-defect`.
- Fixture category: `generated`.
- Policy layer: `standard-strict`.
- Intended current contract: MVR export must embed canonical patched GDTF without reproducing malformed source structures.
- Relevant GDTF/MVR requirement or internal Perastage contract: MVR 1.6 archive plus GDTF 1.2 embedded fixture contract.
- Shared root-cause group: GDTF mutation/canonical publication.
- Confidence: `medium`.
- Evidence supporting the category: Fails with other GDTF mutation tests on both full-suite platforms.
- Evidence still missing: Exact embedded archive diff missing.
- Recommended follow-up phase and action: Phase 4A: repair shared canonical publication path.

#### `MvrSupportUserDataRoundtrip`

- Exact CTest name: `MvrSupportUserDataRoundtrip`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `MVR export failed during CanonicalizeGdtf entry='Unknown@fixture@Perastage.gdtf' ... canonicalizer reported errors`; final assertion: `exporter.ExportToFile(...) ... line 176`. The intended UserData roundtrip assertion is not reached.
- Source test file and assertion or exit point: `tests/mvr_support_userdata_roundtrip_test.cpp`.
- Production components and call path under test: MVR exporter -> embedded GDTF canonicalizer -> roundtrip test setup before UserData assertion.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^MvrSupportUserDataRoundtrip$' --output-on-failure --verbose`.
- Primary classification: `production-defect`.
- Fixture category: `valid`.
- Policy layer: `standard-strict`.
- Intended current contract: The intended later contract is valid user-data preservation, but the observed contract failure is canonical GDTF publication during export.
- Relevant GDTF/MVR requirement or internal Perastage contract: MVR 1.6 GeneralSceneDescription and user data preservation.
- Shared root-cause group: GDTF mutation/canonical publication.
- Confidence: `provisional`.
- Evidence supporting the category: Both platforms fail at export/canonicalization before user-data comparison, tying this test to the GDTF canonical-publication group.
- Evidence still missing: Canonicalizer errors from the MVR export artifact must be traced before auditing user-data preservation.
- Recommended follow-up phase and action: Phase 4A: repair or classify embedded GDTF canonicalization first; rerun before auditing user-data roundtrip.

#### `MvrFixtureCategoryRoundtrip`

- Exact CTest name: `MvrFixtureCategoryRoundtrip`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `MVR export failed during CanonicalizeGdtf entry='Unknown@fixture@Perastage.gdtf' ... canonicalizer reported errors`; final assertion at line 147. The intended category roundtrip assertion is not reached.
- Source test file and assertion or exit point: `tests/mvr_fixture_category_roundtrip_test.cpp`.
- Production components and call path under test: fixture model -> MVR exporter -> embedded GDTF canonicalizer before category mapping assertion.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^MvrFixtureCategoryRoundtrip$' --output-on-failure --verbose`.
- Primary classification: `production-defect`.
- Fixture category: `valid`.
- Policy layer: `standard-strict`.
- Intended current contract: The intended later contract is category roundtrip, but the observed failure is canonical GDTF publication during export.
- Relevant GDTF/MVR requirement or internal Perastage contract: MVR fixture metadata plus Perastage category contract.
- Shared root-cause group: GDTF mutation/canonical publication.
- Confidence: `provisional`.
- Evidence supporting the category: Both platforms fail before category comparison, so category loss is unproven for this CTest in run 29995392074.
- Evidence still missing: Need canonicalizer errors and a rerun after export succeeds before category roundtrip can be evaluated.
- Recommended follow-up phase and action: Phase 4A first: resolve embedded GDTF canonicalization; then Phase 4B can audit category mapping if still failing.

#### `MvrTrussRoundtripStructure`

- Exact CTest name: `MvrTrussRoundtripStructure`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux: `FindLayerChildList ... Assertion 'uuid != nullptr' failed ... line 147`. Windows: `Assertion failed: uuid != nullptr ... line 147`.
- Source test file and assertion or exit point: `tests/mvr_truss_roundtrip_structure_test.cpp`.
- Production components and call path under test: truss model -> MVR GeneralSceneDescription hierarchy -> importer.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^MvrTrussRoundtripStructure$' --output-on-failure --verbose`.
- Primary classification: `production-defect`.
- Fixture category: `valid`.
- Policy layer: `standard-strict`.
- Intended current contract: Truss structure and references must roundtrip in canonical MVR hierarchy.
- Relevant GDTF/MVR requirement or internal Perastage contract: MVR 1.6 scene hierarchy/reference contract.
- Shared root-cause group: MVR GeneralSceneDescription hierarchy/references.
- Confidence: `medium`.
- Evidence supporting the category: Cross-platform strict MVR structural failure.
- Evidence still missing: Exact hierarchy diff missing.
- Recommended follow-up phase and action: Phase 4C: repair hierarchy/reference writer before dependent roundtrips.

#### `MvrExporterCompliance`

- Exact CTest name: `MvrExporterCompliance`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux: `MVR export failed during CanonicalizeGdtf entry='Unknown@Same@Perastage.gdtf' ... canonicalizer reported errors`. Windows: `MVR export failed during CanonicalizeGdtf entry='Unknown@caseonly@Perastage.gdtf' ... canonicalizer reported errors`; final assertion at line 521.
- Source test file and assertion or exit point: `tests/mvr_exporter_compliance_test.cpp`.
- Production components and call path under test: MVR exporter -> UUID/scene identity/archive compliance checks.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^MvrExporterCompliance$' --output-on-failure --verbose`.
- Primary classification: `production-defect`.
- Fixture category: `generated`.
- Policy layer: `standard-strict`.
- Intended current contract: Perastage-generated MVR must be strict canonical MVR 1.6.
- Relevant GDTF/MVR requirement or internal Perastage contract: MVR 1.6 strict output contract.
- Shared root-cause group: MVR malformed UUID/scene identity compliance.
- Confidence: `high`.
- Evidence supporting the category: Exporter compliance is generated output and fails on both Linux and Windows.
- Evidence still missing: Need exact validator complaint from artifact.
- Recommended follow-up phase and action: Phase 4C: first MVR writer compliance branch after GDTF publication group.

#### `SaveLoadRoundtrip`

- Exact CTest name: `SaveLoadRoundtrip`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion ... categoryPropagationA.has_value() ... line 98`.
- Source test file and assertion or exit point: `tests/save_load_roundtrip_test.cpp`.
- Production components and call path under test: category propagation setup -> project serializer/loader path only after categoryPropagationA exists.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^SaveLoadRoundtrip$' --output-on-failure --verbose`.
- Primary classification: `production-defect`.
- Fixture category: `generated`.
- Policy layer: `perastage-extension`.
- Intended current contract: Perastage project save/load must preserve internal scene state deterministically.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal project persistence contract.
- Shared root-cause group: Category propagation and fallback contract.
- Confidence: `medium`.
- Evidence supporting the category: The failure occurs before generic save/load comparison and specifically blocks on category propagation setup.
- Evidence still missing: Need categoryPropagationA setup trace and dictionary/category fallback evidence.
- Recommended follow-up phase and action: Phase 4B: trace category propagation before changing project persistence.

#### `ProjectSupportUserDataRoundtrip`

- Exact CTest name: `ProjectSupportUserDataRoundtrip`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows first production diagnostic: `MVR export failed during ValidateLayers entry='GeneralSceneDescription.xml': Layer UUID is malformed`; final assertion: `cfg.SaveProject(projectPath.string()) ... line 102`.
- Source test file and assertion or exit point: `tests/project_support_userdata_roundtrip_test.cpp`.
- Production components and call path under test: Project save -> MVR scene serialization -> ValidateLayers for GeneralSceneDescription.xml before support UserData assertion.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^ProjectSupportUserDataRoundtrip$' --output-on-failure --verbose`.
- Primary classification: `production-defect`.
- Fixture category: `generated`.
- Policy layer: `perastage-extension`.
- Intended current contract: Project save must serialize a scene with canonical, well-formed layer UUIDs before support user data can be roundtripped.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal Perastage project support data contract.
- Shared root-cause group: Category propagation and fallback contract.
- Confidence: `medium`.
- Evidence supporting the category: The first proven root cause is malformed layer UUID during MVR serialization, not support data loss.
- Evidence still missing: Need the malformed layer UUID source and whether it is generated scene identity or stale fixture setup.
- Recommended follow-up phase and action: Phase 4C: repair malformed scene/layer identity, then rerun support user-data roundtrip.

#### `GdtfShareSecurity`

- Exact CTest name: `GdtfShareSecurity`.
- Affected platform or platforms: Windows.
- First meaningful failure per affected platform: Windows fake-backend save, clear, unavailable-store, and migration operations log success; remaining output begins with `Detected memory leaks!`.
- Source test file and assertion or exit point: `tests/gdtf_share_security_test.cpp`.
- Production components and call path under test: GDTF Share fake backend security test -> Windows Debug CRT leak detection at process exit.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^GdtfShareSecurity$' --output-on-failure --verbose`.
- Primary classification: `platform-specific-contract`.
- Fixture category: `not-applicable`.
- Policy layer: `platform-specific`.
- Intended current contract: The fake-backend security behavior is observed passing; the current failure contract is Windows Debug lifecycle/leak cleanup.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal secure-store policy; platform-specific credential APIs.
- Shared root-cause group: Windows secure-store/debug lifecycle.
- Confidence: `medium`.
- Evidence supporting the category: Windows artifact shows the functional sequence completes and the first failing exit cause is the debug leak report.
- Evidence still missing: Need allocation stack or ownership trace for leaked objects.
- Recommended follow-up phase and action: Phase 4F: repair or suppress only proven test-owned Windows Debug leak after ownership trace.

#### `RiderTrussDictionaryNormalization`

- Exact CTest name: `RiderTrussDictionaryNormalization`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion '!truss.symbolFile.empty()' failed ... line 92`.
- Source test file and assertion or exit point: `tests/rider_truss_dictionary_normalization_test.cpp`.
- Production components and call path under test: rider text importer -> normalized rider model -> scene placement/preview.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^RiderTrussDictionaryNormalization$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `valid`.
- Policy layer: `perastage-extension`.
- Intended current contract: Rider parsing should preserve useful user-authored intent while following documented normalization and placement rules.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal text-to-scene/rider normalization contract; no external standard.
- Shared root-cause group: Rider parser expectations versus current normalization.
- Confidence: `medium`.
- Evidence supporting the category: All rider tests fail cross-platform, suggesting deterministic parser contract drift rather than platform behavior.
- Evidence still missing: Exact expected/current parsed model diff missing.
- Recommended follow-up phase and action: Phase 4D: update text_to_scene_rules.md then fix parser or expectations as one rider contract branch.

#### `RiderImportLinearOrder`

- Exact CTest name: `RiderImportLinearOrder`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion std::abs(lx1Fixtures[i]->transform.o[1] - expectedYOffsets[i]) < 1e-3f ... line 51`.
- Source test file and assertion or exit point: `tests/rider_import_linear_order_test.cpp`.
- Production components and call path under test: rider text importer -> normalized rider model -> scene placement/preview.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^RiderImportLinearOrder$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `valid`.
- Policy layer: `perastage-extension`.
- Intended current contract: Rider parsing should preserve useful user-authored intent while following documented normalization and placement rules.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal text-to-scene/rider normalization contract; no external standard.
- Shared root-cause group: Rider parser expectations versus current normalization.
- Confidence: `medium`.
- Evidence supporting the category: All rider tests fail cross-platform, suggesting deterministic parser contract drift rather than platform behavior.
- Evidence still missing: Exact expected/current parsed model diff missing.
- Recommended follow-up phase and action: Phase 4D: update text_to_scene_rules.md then fix parser or expectations as one rider contract branch.

#### `RiderFilterPreview`

- Exact CTest name: `RiderFilterPreview`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Unexpected filtered preview output.` The retained expected/actual text differs primarily in blank-line structure.
- Source test file and assertion or exit point: `tests/rider_filter_preview_test.cpp`.
- Production components and call path under test: rider text importer -> normalized rider model -> scene placement/preview.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^RiderFilterPreview$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `valid`.
- Policy layer: `perastage-extension`.
- Intended current contract: Rider parsing should preserve useful user-authored intent while following documented normalization and placement rules.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal text-to-scene/rider normalization contract; no external standard.
- Shared root-cause group: Rider parser expectations versus current normalization.
- Confidence: `medium`.
- Evidence supporting the category: All rider tests fail cross-platform, suggesting deterministic parser contract drift rather than platform behavior.
- Evidence still missing: Exact expected/current parsed model diff missing.
- Recommended follow-up phase and action: Phase 4D: update text_to_scene_rules.md then fix parser or expectations as one rider contract branch.

#### `RiderComments`

- Exact CTest name: `RiderComments`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion preview == expectedPreview ... line 30`.
- Source test file and assertion or exit point: `tests/rider_comments_test.cpp`.
- Production components and call path under test: rider text importer -> normalized rider model -> scene placement/preview.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^RiderComments$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `valid`.
- Policy layer: `perastage-extension`.
- Intended current contract: Rider parsing should preserve useful user-authored intent while following documented normalization and placement rules.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal text-to-scene/rider normalization contract; no external standard.
- Shared root-cause group: Rider parser expectations versus current normalization.
- Confidence: `medium`.
- Evidence supporting the category: All rider tests fail cross-platform, suggesting deterministic parser contract drift rather than platform behavior.
- Evidence still missing: Exact expected/current parsed model diff missing.
- Recommended follow-up phase and action: Phase 4D: update text_to_scene_rules.md then fix parser or expectations as one rider contract branch.

#### `RiderLedScreenObject`

- Exact CTest name: `RiderLedScreenObject`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion std::fabs(screenScale.u[0] - (8.0f / 0.3f)) < kTolerance ... line 41`.
- Source test file and assertion or exit point: `tests/rider_led_screen_object_test.cpp`.
- Production components and call path under test: rider text importer -> normalized rider model -> scene placement/preview.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^RiderLedScreenObject$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `valid`.
- Policy layer: `perastage-extension`.
- Intended current contract: Rider parsing should preserve useful user-authored intent while following documented normalization and placement rules.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal text-to-scene/rider normalization contract; no external standard.
- Shared root-cause group: Rider parser expectations versus current normalization.
- Confidence: `medium`.
- Evidence supporting the category: All rider tests fail cross-platform, suggesting deterministic parser contract drift rather than platform behavior.
- Evidence still missing: Exact expected/current parsed model diff missing.
- Recommended follow-up phase and action: Phase 4D: update text_to_scene_rules.md then fix parser or expectations as one rider contract branch.

#### `RiderHoistImport`

- Exact CTest name: `RiderHoistImport`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion countByPosition["SIDEFILL"] == 2 ... line 55`.
- Source test file and assertion or exit point: `tests/rider_hoist_import_test.cpp`.
- Production components and call path under test: rider text importer -> normalized rider model -> scene placement/preview.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^RiderHoistImport$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `valid`.
- Policy layer: `perastage-extension`.
- Intended current contract: Rider parsing should preserve useful user-authored intent while following documented normalization and placement rules.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal text-to-scene/rider normalization contract; no external standard.
- Shared root-cause group: Rider parser expectations versus current normalization.
- Confidence: `medium`.
- Evidence supporting the category: All rider tests fail cross-platform, suggesting deterministic parser contract drift rather than platform behavior.
- Evidence still missing: Exact expected/current parsed model diff missing.
- Recommended follow-up phase and action: Phase 4D: update text_to_scene_rules.md then fix parser or expectations as one rider contract branch.

#### `RiderLxSidesImport`

- Exact CTest name: `RiderLxSidesImport`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion NearlyEqual(truss.transform.o[2], 5000.0f) ... line 46`.
- Source test file and assertion or exit point: `tests/rider_lx_sides_import_test.cpp`.
- Production components and call path under test: rider text importer -> normalized rider model -> scene placement/preview.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^RiderLxSidesImport$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `valid`.
- Policy layer: `perastage-extension`.
- Intended current contract: Rider parsing should preserve useful user-authored intent while following documented normalization and placement rules.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal text-to-scene/rider normalization contract; no external standard.
- Shared root-cause group: Rider parser expectations versus current normalization.
- Confidence: `medium`.
- Evidence supporting the category: All rider tests fail cross-platform, suggesting deterministic parser contract drift rather than platform behavior.
- Evidence still missing: Exact expected/current parsed model diff missing.
- Recommended follow-up phase and action: Phase 4D: update text_to_scene_rules.md then fix parser or expectations as one rider contract branch.

#### `RiderPipeImport`

- Exact CTest name: `RiderPipeImport`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion lx1Count == 1 ... line 91`.
- Source test file and assertion or exit point: `tests/rider_pipe_import_test.cpp`.
- Production components and call path under test: rider text importer -> normalized rider model -> scene placement/preview.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^RiderPipeImport$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `valid`.
- Policy layer: `perastage-extension`.
- Intended current contract: Rider parsing should preserve useful user-authored intent while following documented normalization and placement rules.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal text-to-scene/rider normalization contract; no external standard.
- Shared root-cause group: Rider parser expectations versus current normalization.
- Confidence: `medium`.
- Evidence supporting the category: All rider tests fail cross-platform, suggesting deterministic parser contract drift rather than platform behavior.
- Evidence still missing: Exact expected/current parsed model diff missing.
- Recommended follow-up phase and action: Phase 4D: update text_to_scene_rules.md then fix parser or expectations as one rider contract branch.

#### `Viewer2DFboCaptureDiagnostics`

- Exact CTest name: `Viewer2DFboCaptureDiagnostics`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `RenderToRGBA must record fallback usage with the FBO diagnostic reason`.
- Source test file and assertion or exit point: `tests/check_viewer2d_fbo_capture_diagnostics.sh:112`.
- Production components and call path under test: Viewer2D RenderToRGBA fallback diagnostic integration -> source-pattern policy check.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^Viewer2DFboCaptureDiagnostics$' --output-on-failure --verbose`.
- Primary classification: `test-harness-defect`.
- Fixture category: `not-applicable`.
- Policy layer: `not-applicable`.
- Intended current contract: RenderToRGBA fallback paths should record FBO diagnostic reasons, unless the policy source pattern is stale relative to an equivalent diagnostic call.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal diagnostic-policy test contract.
- Shared root-cause group: Viewer2D fallback diagnostic policy.
- Confidence: `medium`.
- Evidence supporting the category: The observed evidence is a source-policy failure; it does not yet prove whether production diagnostics are missing or the policy pattern is stale.
- Evidence still missing: Need source trace from RenderToRGBA fallback paths to determine if diagnostic recording exists under a different helper.
- Recommended follow-up phase and action: Phase 4G: trace Viewer2D fallback diagnostic call sites before changing policy or production.

#### `EditableFocusUtils`

- Exact CTest name: `EditableFocusUtils`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux system-out includes `AT-SPI: Error retrieving accessibility bus address ... org.a11y.Bus ...`; Windows system-out is empty. No differentiated assertion is retained in the JUnit evidence.
- Source test file and assertion or exit point: `tests/editable_focus_utils_test.cpp`.
- Production components and call path under test: GUI editable-focus utility -> wxWidgets focus/window lifecycle.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^EditableFocusUtils$' --output-on-failure --verbose`.
- Primary classification: `test-harness-defect`.
- Fixture category: `not-applicable`.
- Policy layer: `platform-specific`.
- Intended current contract: Headless GUI tests must create/destroy wx objects deterministically and assert utility behavior independent of desktop services.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal GUI utility contract; wx lifecycle constraints.
- Shared root-cause group: wxWidgets lifecycle/headless focus.
- Confidence: `provisional`.
- Evidence supporting the category: Fails on both platforms; Linux AT-SPI/DBus warning is non-causal because Windows fails without it, but Windows provides no differentiated assertion output.
- Evidence still missing: Exact assertion missing; need focused local GUI environment.
- Recommended follow-up phase and action: Phase 4F: isolate wx lifecycle from utility assertions.

#### `LayoutTemplatePackageService`

- Exact CTest name: `LayoutTemplatePackageService`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion ... imageEntryCount == 1 ... line 141`.
- Source test file and assertion or exit point: `tests/layout_template_package_service_test.cpp:107`.
- Production components and call path under test: LayoutTemplatePackageService -> image registry -> zip/json portable paths.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^LayoutTemplatePackageService$' --output-on-failure --verbose`.
- Primary classification: `production-defect`.
- Fixture category: `generated`.
- Policy layer: `perastage-extension`.
- Intended current contract: Layout template packages must use portable normalized paths and preserve registered resources.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal portable layout package contract.
- Shared root-cause group: Layout portable path/resource normalization.
- Confidence: `medium`.
- Evidence supporting the category: Source asserts export/import/validate cycles and shares domain with registry failure.
- Evidence still missing: Exact assertion line from CI missing.
- Recommended follow-up phase and action: Phase 4E: repair package path/resource contract.

#### `LayoutImageResourceRegistry`

- Exact CTest name: `LayoutImageResourceRegistry`.
- Affected platform or platforms: Windows.
- First meaningful failure per affected platform: Windows: `Assertion failed: entries.find(storedFirstResourcePath) != entries.end() ... line 132`, followed by CRT leak output.
- Source test file and assertion or exit point: `tests/layout_image_resource_registry_test.cpp:86`.
- Production components and call path under test: LayoutImageResourceRegistry singleton -> path normalization/dedup usage counts.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^LayoutImageResourceRegistry$' --output-on-failure --verbose`.
- Primary classification: `platform-specific-contract`.
- Fixture category: `generated`.
- Policy layer: `platform-specific`.
- Intended current contract: Resource keys must normalize Windows paths and Unicode consistently without changing portable package semantics.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal layout resource registry path contract.
- Shared root-cause group: Layout portable path/resource normalization.
- Confidence: `medium`.
- Evidence supporting the category: Windows-only; source checks UsageCount and UsedResources.
- Evidence still missing: Exact path values missing.
- Recommended follow-up phase and action: Phase 4E: fix Windows normalization after package contract.

#### `PdfTextComparison`

- Exact CTest name: `PdfTextComparison`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux: `translated.pdf` expected `Foo Bar`, actual `FooBar`. Windows first reports `PdfErrorCode::InvalidDataType` and `Unsupported PdfContentType`, then expected `Foo Bar`, actual empty.
- Source test file and assertion or exit point: `tests/pdf_text_comparison_test.cpp`.
- Production components and call path under test: PDF text extraction/comparison -> PoDoFo parser handling of translated.pdf content streams.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^PdfTextComparison$' --output-on-failure --verbose`.
- Primary classification: `stale-expectation`.
- Fixture category: `generated`.
- Policy layer: `perastage-extension`.
- Intended current contract: PDF comparison must first distinguish invalid/obsolete fixture content from a PoDoFo compatibility issue or production extraction defect.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal PDF serialization/text comparison contract.
- Shared root-cause group: PDF formatting versus brittle text expectations.
- Confidence: `medium`.
- Evidence supporting the category: The first failure is a PoDoFo data-type/content-type error, so stale text expectation is only a follow-up hypothesis.
- Evidence still missing: Need fixture validity review for translated.pdf and PoDoFo compatibility notes before changing expectations.
- Recommended follow-up phase and action: Phase 4H: validate translated.pdf fixture and PoDoFo support path before changing golden text.

#### `PdfWriterSerialization`

- Exact CTest name: `PdfWriterSerialization`.
- Affected platform or platforms: Windows.
- First meaningful failure per affected platform: Windows: JUnit system-out is empty; no retained assertion or diagnostic identifies the cause.
- Source test file and assertion or exit point: `tests/pdf_writer_test.cpp`.
- Production components and call path under test: PDF writer serialization -> file output/runtime cleanup.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^PdfWriterSerialization$' --output-on-failure --verbose`.
- Primary classification: `platform-specific-contract`.
- Fixture category: `generated`.
- Policy layer: `platform-specific`.
- Intended current contract: PDF writer test must serialize deterministically on Windows and avoid CRT/lifecycle false failures.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal PDF writer contract plus Windows runtime behavior.
- Shared root-cause group: Windows PDF writer/runtime lifecycle.
- Confidence: `provisional`.
- Evidence supporting the category: Windows-only with sparse output; requested inspection treats exit path separately.
- Evidence still missing: Generated files/debug diagnostics missing locally.
- Recommended follow-up phase and action: Phase 4H/F: reproduce on Windows and distinguish assertion from CRT cleanup.

#### `TrussPathEncodingRegression`

- Exact CTest name: `TrussPathEncodingRegression`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux: `Assertion 'ProjectUtils::LoadLastProjectPath() == expectedPath' failed ... line 120`. Windows: no assertion in JUnit; output contains invalid UTF-8-related allocations and `Detected memory leaks!`.
- Source test file and assertion or exit point: `tests/truss_path_encoding_regression_test.cpp`.
- Production components and call path under test: ProjectUtils last-project-path persistence -> UTF-8/path conversion -> ConfigManager.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^TrussPathEncodingRegression$' --output-on-failure --verbose`.
- Primary classification: `platform-specific-contract`.
- Fixture category: `generated`.
- Policy layer: `platform-specific`.
- Intended current contract: Last project path persistence must roundtrip Unicode paths using a documented portable encoding.
- Relevant GDTF/MVR requirement or internal Perastage contract: Internal project path encoding/persistence contract.
- Shared root-cause group: Project path encoding and persistence.
- Confidence: `medium`.
- Evidence supporting the category: Explicitly divergent Linux/Windows symptoms require platform-specific classification until Windows exit is proven.
- Evidence still missing: Need Windows assertion vs CRT leak determination.
- Recommended follow-up phase and action: Phase 4F: reproduce with debug leak settings and path traces.

#### `Loader3dsNativeDimensions`

- Exact CTest name: `Loader3dsNativeDimensions`.
- Affected platform or platforms: Linux, Windows.
- First meaningful failure per affected platform: Linux/Windows: `Assertion NearlyEqual(MeshAxisSize(nativeMesh, 2), 290.0f) ... line 129`.
- Source test file and assertion or exit point: `tests/loader3ds_native_dimensions_test.cpp`.
- Production components and call path under test: 3DS loader -> axis conversion/native dimensions calculation.
- Exact focused reproduction command: `ctest --test-dir <build-dir> -R '^Loader3dsNativeDimensions$' --output-on-failure --verbose`.
- Primary classification: `standards-ambiguity-needing-documentation`.
- Fixture category: `valid`.
- Policy layer: `not-applicable`.
- Intended current contract: 3DS dimensions must state whether native file axes or Perastage scene axes are authoritative.
- Relevant GDTF/MVR requirement or internal Perastage contract: 3DS is legacy binary geometry; internal axis/dimension convention must be documented.
- Shared root-cause group: 3DS axis/dimension convention.
- Confidence: `medium`.
- Evidence supporting the category: Cross-platform deterministic geometry mismatch with no policy document cited.
- Evidence still missing: Need fixture authoring convention and expected axis basis.
- Recommended follow-up phase and action: Phase 4I: document convention then adjust loader or test.

### Phase 3 reconciliation, counts, and repair order

Distinct current failures classified: 29. Linux and Windows share 25 failures; Windows has 4 additional domain failures (`PdfWriterSerialization`, `GdtfFixtureInsertionPreparation`, `LayoutImageResourceRegistry`, and `GdtfShareSecurity`). `ReleaseGatePolicyPortability` is fixed and excluded. Linux `CredentialStoreNativeRoundTrip` is documented as the single skip and excluded from failures. No production behavior, test assertion, fixture, CMake, workflow, label, timeout, skip, packaging, reader, writer, rider, layout, PDF, geometry, path, or GUI file is changed by this Phase 3 documentation task.

Classification counts by primary category:

- `production-defect`: 10.
- `stale-expectation`: 11.
- `platform-specific-contract`: 5.
- `test-harness-defect`: 2.
- `standards-ambiguity-needing-documentation`: 1.
- `invalid-or-obsolete-fixture`: 0.
- `flaky-or-nondeterministic`: 0.
- `duplicate-or-no-longer-useful`: 0.

Root-cause groups ordered by expected Phase 4 impact and dependency:

1. GDTF mutation/canonical publication: `GdtfLoaderSetPropertiesMutation`, `SymbolFixtureApplierGdtfMutation`, `MvrPatchedGdtfExportMutation`. Best first Phase 4 branch because it protects the strict-output side of both GDTF and MVR workflows.
2. MVR malformed UUID/scene identity and GeneralSceneDescription preservation: `MvrExporterCompliance`, `MvrSupportUserDataRoundtrip`, `MvrTrussRoundtripStructure`.
3. Category propagation and project persistence: `GdtfFixtureCategoryFallback`, `MvrFixtureCategoryRoundtrip`, `SaveLoadRoundtrip`, `ProjectSupportUserDataRoundtrip`.
4. Rider parser expectations versus current normalization: all eight rider tests; update `docs/developer/text_to_scene_rules.md` before behavior or assertion changes.
5. Layout portable path/resource normalization: `LayoutTemplatePackageService`, `LayoutImageResourceRegistry`.
6. Windows/platform lifecycle and path contracts: `GdtfFixtureInsertionPreparation`, `GdtfShareSecurity`, `PdfWriterSerialization`, `TrussPathEncodingRegression`, `EditableFocusUtils`.
7. PDF formatting contract: `PdfTextComparison`, then `PdfWriterSerialization` if reproduction proves writer semantics rather than Windows lifecycle.
8. Viewer2D fallback diagnostic policy: `Viewer2DFboCaptureDiagnostics`.
9. 3DS axis/dimension convention: `Loader3dsNativeDimensions`.

Recommended branch boundaries: keep each root-cause group in its own Phase 4 branch; do not mix MVR/GDTF strict-output fixes with rider parser expectation updates or Windows lifecycle work. Remaining uncertainties are the exact XML/text/path diffs from downloadable artifacts, Windows CRT leak-versus-assertion exits for `TrussPathEncodingRegression` and `PdfWriterSerialization`, and the normative 3DS axis convention. Safe Merge Point C decision: not reached yet. The audit now reconciles the 29 entries, but several entries remain provisional until the downloadable artifact segments, Windows exit paths, and source-level diagnostic traces are attached and reviewed; this decision does not authorize Phase 4 implementation.

## Phase 4A D1 evidence: GDTF mutation and canonical publication

Baseline for this branch: local `main` was available at `9acdfed2e9f49b11c8579bbb996d41ee68a7447c`; network fetch could not be used because no `origin` remote is configured in the execution checkout. The diff between merged task head `7a31d1c86a159bef739a6b5beec2e38845fd69da` and reviewed main `9acdfed2e9f49b11c8579bbb996d41ee68a7447c` is limited to `VERSION`, confirming the automatic version bump in this checkout.

Gate 1 evidence in this local environment is limited to source and local configure inspection: the merged Bash/PowerShell harness changes remain present, no test was skipped or converted to continue-on-error by this branch, and the mandatory policy tests remain required. Full cross-platform CI artifacts for run `30015091475` must be attached by GitHub Actions before final merge because this checkout has no configured GitHub remote and cannot inspect workflow artifacts directly.

GDTF 1.2 XSD conclusion used for Phase 4A: the authoritative GDTF 1.2 schema referenced by the GDTF Hub defines `FixtureType` children as an ordered `xs:sequence`; `AttributeDefinitions`, `Geometries`, and `DMXModes` each have `minOccurs="1"`, while `Wheels`, `PhysicalDescriptions`, `Models`, `Revisions`, `FTPresets`, and `Protocols` have `minOccurs="0"`. The same `FixtureType` complex type marks `Name`, `Manufacturer`, `Description`, and `FixtureTypeID` as required attributes. Therefore the previous ad-hoc strict mutation fixtures were invalid/obsolete strict fixtures, although tolerant readers may still recover useful data from them.

Affected test classification:

| Test | Old classification | First local/root evidence | Final classification | Repair |
|---|---|---|---|---|
| `GdtfLoaderSetPropertiesMutation` | production-defect candidate | Test fixture omitted required `FixtureTypeID`, `Description`, `AttributeDefinitions`, and `DMXModes`; canonicalizer rejected publication after mutation. | mixed root cause: invalid-or-obsolete-fixture plus production diagnostic/publication weakness | Replaced the strict fixture with the shared canonical GDTF 1.2 test builder and routed mutation through structured diagnostics plus sibling temporary archive publication. |
| `SymbolFixtureApplierGdtfMutation` | production-defect candidate | Test fixture used the same incomplete strict shape; symbol mutation expected strict output from invalid input. | invalid-or-obsolete-fixture | Replaced the strict fixture with the shared canonical GDTF 1.2 test builder while keeping legacy `PerastageMutationAudit` compatibility fixtures separate. |
| `MvrPatchedGdtfExportMutation` | production-defect candidate | Test source GDTF omitted required strict fields/sections before MVR export patching. | invalid-or-obsolete-fixture | Replaced the strict source fixture with the shared canonical GDTF 1.2 test builder; export canonicalization remains on the derived embedded copy and does not rewrite the source archive. |

Focused command used in this environment: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target gdtf_canonicalizer_test gdtfloader_set_properties_test symbol_fixture_applier_gdtf_test mvr_patched_gdtf_export_test -j2`. Result: configure failed before compilation because wxWidgets development files are not installed in the container (`Could NOT find wxWidgets`). No test result was hidden, skipped, disabled, or converted to continue-on-error.

D1 status: not fully reached in this container because cross-platform GitHub Actions evidence and local wxWidgets-backed execution are unavailable here. The source repair keeps input tolerance unchanged, corrects the strict fixtures, adds mutation diagnostics for the viewer3d mutation path, and replaces direct GDTF rewrite with sibling temporary archive publication before atomic replacement.

### Follow-up after CI run 30027673580

Authoritative validation run for reviewed head `2693f9058e4a2e68f79b39e41121ba01dacf301f`: `30027673580`.

Platform evidence from that run:

- Linux Debug Tests configured and built successfully; result totals were 161 total, 136 passed, 24 failed, and 1 skipped.
- Windows Debug Tests configured and built successfully; result totals were 163 total, 135 passed, and 28 failed.
- macOS configured, built, and passed the current reduced release-gate profile with 6 passed and 0 failed. This is not full macOS test parity.
- `GitBashResolverContract` passed on Linux and Windows.
- `PowerShellNativeCaptureWindowsPowerShell` and `PowerShellNativeCapturePowerShell7` passed on Windows.
- `GdtfCanonicalizerExportRules` and `GdtfTestFixtureBuilder` passed on Linux and Windows.
- `MvrPatchedGdtfExportMutation` passed on Linux and Windows after the strict fixture repair.
- `GdtfLoaderSetPropertiesMutation` still failed only on the stale lexical float assertion for `PowerConsumption Value="678.900"`; mutation success, changed status, empty errors, and atomic replacement were already reached.
- `SymbolFixtureApplierGdtfMutation` still failed at `ApplySymbolsToFixtureGdtf(...)` because the test used an arbitrary absolute temporary file with no project base path while requesting a scene-copy-only mutation.

Safe Merge Point C is verified for classification and harness purposes: the Bash resolver and PowerShell native-capture infrastructure is stable across the covered platforms, Linux/Windows configure and build are functional, the macOS reduced release-gate profile is functional, and no new harness regression is visible. Provisional domain classifications remain refinable inside their focused Phase 4 repair branches.

Phase 4A D1 follow-up classifications:

| Test | Before run `30027673580` | Evidence from run `30027673580` | Follow-up repair | Expected D1 status |
|---|---|---|---|---|
| `GdtfLoaderSetPropertiesMutation` | Mixed fixture/production-publication candidate. | Publication succeeded; only exact trailing-zero float spelling failed. | Compare GDTF Float values numerically with strict token validation, preserve sentinel resource bytes, validate final archive, and prove injected publication failure preserves original bytes. | Should pass for standards-based semantic reasons. |
| `SymbolFixtureApplierGdtfMutation` | Invalid/obsolete fixture candidate. | Strict fixture was repaired, but setup violated source ownership by using an external absolute path with no project base. | Use a temporary project base and project-relative source so production resolves or creates a writable project-owned scene copy; emit the service diagnostic before assertion. | Should pass without weakening external/library ownership. |
| `MvrPatchedGdtfExportMutation` | Invalid/obsolete fixture. | Passed on Linux and Windows. | No additional MVR/export production change in this follow-up. | Remains passing. |

Publication failure-preservation evidence added in source: `GdtfLoaderSetPropertiesMutation` now injects a deterministic failure before atomic replacement, asserts `success == false`, checks that structured errors name `BeforeAtomicReplace`, verifies the original archive bytes are unchanged, and verifies no unique sibling mutation temporary archive remains. The success path also verifies unrelated resource bytes survive mutation and the final archive validates through the canonicalizer export rules.

Schema cross-check: the shared strict fixture shape was compared against the fetched `mvrdevelopment/tools:gdtf.xsd` artifact with Python `lxml.etree.XMLSchema`; after adding the required `ChannelFunction Default="0/1"` attribute, the generated minimal fixture structure validates against that XSD. This check validates the standard-strict default fixture shape only; the category-signal helper remains a test extension helper and is not used as the baseline strict fixture.


## Phase 4A urgent-hotfix reconciliation and residual repair, 2026-07-23

Current task branch: `codex/resume-test-repair-after-urgent-hotfix`. The execution checkout has no configured `origin` remote, so remote-main fetching and GitHub Actions artifact download were not available from this container. The current checked-out main-equivalent SHA used as the branch base is `48cc1b758026d4d0041787964de028cb9870db87`. The urgent repair merge/commit SHA visible in history is `48cc1b758026d4d0041787964de028cb9870db87` (`Merge pull request #2207 from PeramatoG/codex/fix-macos-version-header-collision-and-shutdown-crash`), with parent repair commit `5b2f0fd`.

Diff inspection from final Phase 4A task head `df59c8d46ca421d0f84d7d647075dc88dcfd36b6` to `48cc1b758026d4d0041787964de028cb9870db87` showed changes in `VERSION`, `core/diagnostics/CrashHandler.*`, `core/runtime_storage.*`, `docs/release-notes-draft.md`, `main.cpp`, `tests/CMakeLists.txt`, `tests/check_test_targets_no_source_root_includes.py`, `tests/runtime_storage_test.cpp`, `viewer3d/gdtfloader.cpp`, and `viewer3d/gdtfloader.h`. Therefore the urgent repair did touch Phase 4A files and dependencies (`viewer3d/gdtfloader.cpp`, `viewer3d/gdtfloader.h`, `tests/CMakeLists.txt`, and runtime-storage helpers), so old Phase 4A run `30051025506` is not sufficient evidence for current main. The run status and artifacts for `30051025506` could not be inspected in this checkout because no GitHub remote is configured; configure, build, CTest, inventory, JUnit, compact JSON, failure CSV, diagnostics, Linux/Windows totals, and reduced macOS profile totals remain pending external CI artifact attachment.

Local Gate A focused verification on branch SHA `48cc1b758026d4d0041787964de028cb9870db87` initially reproduced one residual Phase 4A failure: `GdtfLoaderSetPropertiesMutation` failed its semantic float assertion after publication because binary `float` formatting exposed noise for the requested `PowerConsumption` value (`678.9f`). This was a residual production serialization defect: the mutation publication path succeeded, preserved archive entries, and reached atomic replacement, but the output spelling did not represent the user-level decimal value cleanly enough for strict semantic comparison.

Residual Phase 4A repair: `core/gdtf_mutation_audit.cpp` now formats GDTF physical-property mutation floats through one helper before setting XML attributes, avoiding binary float noise in strict GDTF output while keeping the existing mutation logic and audit path unchanged. After the repair, the focused Gate A tests passed locally: `GdtfCanonicalizerExportRules`, `GdtfTestFixtureBuilder`, `ProjectFixtureGdtfApplyAdapter`, `GdtfLoaderSetPropertiesMutation`, `SymbolFixtureApplierGdtfMutation`, and `MvrPatchedGdtfExportMutation`.

Gate decision: Gate A failed at the beginning of this task because a Phase 4A focused test failed on current main. Per the task stop rule, this branch is a focused Phase 4A follow-up PR only. UUID and scene-identity Phase 4B work was not started. The current MVR UUID failures remain reproduced but intentionally unrepaired here: `ProjectSupportUserDataRoundtrip` still fails during `ValidateLayers` with `Layer UUID is malformed`; `MvrTrussRoundtripStructure` still fails because the exported layer child-list lookup has no canonical layer `uuid`; `MvrExporterCompliance` still has a GDTF canonicalization blocker in its broader MVR export fixture.


## Phase 4A D1 Windows archive path portability follow-up, 2026-07-24

Current reviewed branch head before this follow-up: `231e69560ab13bb28e9c86f01b55ff388f2e2773`. Current branch base/main from the task evidence: `768ac8dcf8cf7eb266ef308629e5e32858c260f3`. Authoritative CI run for that head: `30052763033`. That run configured and built successfully on Linux and Windows; Linux CTest reported 162 total, 137 passed, 24 failed, and 1 skipped; Windows CTest reported 164 total, 134 passed, and 30 failed; the reduced macOS release-gate profile reported 6 passed and 0 failed. The reduced macOS profile is not full macOS parity.

Before status from run `30052763033`: all six focused Phase 4A tests passed on Linux. On Windows, `GdtfCanonicalizerExportRules`, `GdtfTestFixtureBuilder`, `ProjectFixtureGdtfApplyAdapter`, and `MvrPatchedGdtfExportMutation` passed, while `GdtfLoaderSetPropertiesMutation` failed at `sentinelIt != entries.end()` and `SymbolFixtureApplierGdtfMutation` failed at `entries.find("models/svg/Body.svg") != entries.end()`. The Windows diagnostics showed wx-presented names such as `resources\sentinel.bin`, `models\svg\Body.svg`, `models\svg\Body_bottom.svg`, `models\svg_side\Body.svg`, and `models\svg_front\Body.svg`.

Repair evidence: test archive readers now normalize wx-presented logical entry names through shared test support before comparison, converting native separators to `/` while rejecting absolute paths, drive-qualified paths, empty components, `.`, and `..`. The same tests also inspect the raw ZIP central directory and require canonical stored names with `/`, proving the production publication output remains canonical rather than changing a writer for a Windows wxWidgets presentation difference. Raw-name coverage includes `resources/sentinel.bin`, `models/svg/Body.svg`, `models/svg/Body_bottom.svg`, `models/svg_side/Body.svg`, and `models/svg_front/Body.svg`.

Float policy repair: the earlier six-significant-digit formatter was replaced with locale-independent shortest-roundtrip `std::to_chars` formatting for finite `float` values. Focused tests now cover `0.0f`, a negative finite value, `12.345f`, `678.9f`, a value requiring more than six significant digits to roundtrip, a small finite value, and a large finite value. The emitted tokens are parsed back to `float`, checked for exact bitwise roundtrip, and checked for absence of locale commas. NaN, positive infinity, and negative infinity are rejected before publication with structured errors, unchanged original archive bytes, no sibling mutation temporary archive, and no atomic replacement completion.

Local Linux verification after this follow-up passed `GdtfLoaderSetPropertiesMutation` and `SymbolFixtureApplierGdtfMutation`. Native Windows reproduction and the complete Debug Tests workflow could not be launched from this Linux-only checkout because no GitHub remote is configured in the container; the final branch SHA, follow-up CI run ID, final Linux/Windows totals, final reduced macOS profile result, and D1 acceptance decision must therefore be recorded from PR #2208 CI after this commit is pushed. Remaining domain failures are still the pre-existing non-Phase-4A groups, including MVR UUID/scene identity and unrelated rider/layout/PDF/platform items; Phase 4B was not started.

## Current-main Phase 4A policy-regression closure, 2026-07-26

Branch: `codex/close-phase4a-current-main-policy-regressions`. The exact branch base and initial task-branch SHA is `8e4ef9c3c7218fbf015ae2221fd74513d47105da`, the supplied latest `main`; the repository has no configured remote, so a new fetch was not possible in this checkout. The version at that base is `1.5.9`. The final committed branch SHA and its complete CI run ID must be added from the PR after the commit is pushed, because a commit cannot contain its own SHA and this checkout cannot dispatch GitHub Actions without a remote.

Phase 4A evidence is PR #2208, final task head `e348bbf29385d55de32e1bfc71fc6924d9c8de70`, merge commit `261f56733e3b8d32f9fb97f3def0c97539d9821c`, and final run `30073756568`. That run configured and built successfully on Linux and Windows. Linux CTest reported 162 total, 137 passed, 24 failed, and 1 skipped; Windows CTest reported 164 total, 136 passed, and 28 failed; macOS passed its reduced release-gate profile with 6 passed and 0 failed. The six focused Phase 4A tests passed on Linux and Windows: `GdtfCanonicalizerExportRules`, `GdtfTestFixtureBuilder`, `GdtfLoaderSetPropertiesMutation`, `SymbolFixtureApplierGdtfMutation`, `MvrPatchedGdtfExportMutation`, and `ProjectFixtureGdtfApplyAdapter`.

The current cache/CI evidence is PR #2220, final implementation head `b5094dc4f2705ccf72934d07f801bf8e60a84132`, merge commit `5b4fdee1e1f8f75b4ce69ff6322574a73e19d6dc`, and run `30174559730`. Linux and Windows configure, build, toolchain validation, and sccache stages passed. Linux CTest reported 162 total, 136 passed, 25 failed, and 1 skipped; Windows CTest reported 164 total, 135 passed and 29 failed. macOS configured, built, and retained the reduced 6-passed, 0-failed release-gate profile; this is not full macOS parity. The same six focused Phase 4A tests passed on Linux and Windows. A red CTest suite result in this run therefore does not mean that configuration or compilation failed.

Two repository-policy failures are in scope:

- `WindowsDebugToolPreflight`: `Windows CMake configure must receive the validated Git Bash path.` This was a stale literal-command-line expectation. The workflow instead carries the probed `PERASTAGE_GIT_BASH` value through `$bashExecutable`, `--bash-executable`, a forced `BASH_EXECUTABLE` `FILEPATH` entry in the generated initial cache, CMake `-C`, and post-configure toolchain validation.
- `WxFilesystemPathBoundaries`: `tests/gdtfloader_set_properties_test.cpp` constructed `wxFileInputStream` from `archivePath.string()`. This was a real test-code filesystem boundary defect and now uses `WxPathUtils::WxStringFromFilesystemPath` without changing logical ZIP-name normalization or raw central-directory validation.

The exact failure-set delta from run `30073756568` to run `30174559730` is: Linux added `WindowsDebugToolPreflight`; Windows added `WindowsDebugToolPreflight`; neither platform removed a failure; and `WxFilesystemPathBoundaries` was already failing at the final Phase 4A head. No test registration, label, skip, timeout, exclusion, or domain assertion is changed by this repair.

Local focused policy verification passes the Windows Bash data-flow policy, initial-cache helper unit tests, wx filesystem boundary scanner, CMake toolchain validator policy, CI workflow architecture policy, and source-root include policy from the supported working directories. The wx-backed Phase 4A executables and full cross-platform Debug Tests workflow still require PR CI evidence. The expected post-repair failure-set change, if registration remains unchanged, is removal of `WindowsDebugToolPreflight` and `WxFilesystemPathBoundaries` on both Linux and Windows, with no other addition or removal. vcpkg and sccache workflow behavior is preserved because the workflow and cache helper implementation are unchanged.

D1 remains pending the complete PR CI run and failure-set reconciliation. The remaining failures must stay visible and are the previously classified domain groups covering MVR identity/user-data, rider parsing, layout, PDF, platform/lifecycle, Viewer2D, and 3DS behavior. After D1 is verified and this focused PR is merged, the exact next group is MVR UUID and scene identity. Phase 4B is not started here.

## Phase 4B baseline and canonical MVR identity, 2026-07-26

### D1 merge record

PR #2221, from branch `codex/cierra-regresiones-de-politica-ci-actuales`, was merged as `4f636fe06e381eaaea052f20182a3dfbc84e29c6`. Its verified head was `a0352d4f800033d135469b6519ae5e7b8cfce35e`, and authoritative Debug Tests run `30196121317` reported Linux 162 total, 138 passed, 23 failed, and 1 skipped; Windows 164 total, 137 passed, and 27 failed; and the reduced macOS profile 6 passed of 6. The failure-set change removed exactly `WindowsDebugToolPreflight` and `WxFilesystemPathBoundaries` on Linux and Windows, with no added failure. Intermediate point D1 was reached.

The post-merge main-equivalent checkout is `2d56194f90a36345f1b0767dc888729ee7e96fa5`, version `1.5.10`. No `origin` remote is configured in this execution checkout, so a network fetch was not possible. Local history and `git diff a0352d4f800033d135469b6519ae5e7b8cfce35e..2d56194f90a36345f1b0767dc888729ee7e96fa5` confirm the only post-tested-head content change is `VERSION`; the other difference is merge topology. No production or test code changed, so focused D1 controls did not require rerunning before Phase 4B.

This evidence supersedes the inherited statement under “Current-main Phase 4A policy-regression closure” that D1 and the final PR #2221 run were pending.

### MVR 1.6 identity contract and pipeline audit

The repository reference artifact is `docs/reference/mvr-spec.md`, identified as “MVR Version 1.6 - DIN SPEC 15801”, 132843 bytes, SHA-256 `74f798834176f08b729ccf8c922d3fdf223e4b17c4a6b46946043834d6a1302b`. It records the RFC 4122 text representation `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`, prohibits the nil UUID, and marks UUID attributes required for Layer, Symdef, Position, Fixture, GroupObject, Truss, Support, SceneObject, and Symbol; `Symbol@symdef` targets a Symdef UUID. The prose artifact says Layer permits zero or one ChildList, while the project strict-output contract and authoritative-schema-based test policy require one ChildList for every emitted Layer. Strict Perastage output follows the latter stronger shape. The prose calls each identity unique but does not define one global namespace shared by scene nodes, Symbols, Symdefs, and Positions; recovery therefore enforces scene-object uniqueness together, and separate Layer, Symbol/Symdef, and Position scopes rather than inferring unsupported cross-scope conflicts.

The authoritative public MVR 1.6 schema was reviewed from repository `mvrdevelopment/tools` at commit `16f9ff3624d3e715798a28b2c460579c55820853`, file `mvr.xsd` (Git blob `b250b81a1a98f5dbeaf7eb55c54e21409d83f829`). The downloaded artifact is 28,286 bytes with SHA-256 `c6dadedf91d9bd93148f2fdb3843cfca9c8f8ec0b86f035cbb4110def74af5ef`. It declares `Layer/ChildList` with `minOccurs="0"`; a ChildList is therefore optional for MVR 1.6 validity. Perastage intentionally emits exactly one ChildList per Layer as its stronger deterministic canonical-output policy, not as an XSD requirement. The schema is pinned by repository, commit, blob, size, and digest rather than vendored, avoiding an unnecessary maintained copy and licensing duplication.

Identity flow before this repair duplicated scene identity between unordered-map keys and object `uuid` fields. Layers also retained map key, `Layer::uuid`, and name-backed object assignments; compatibility enumeration synthesized `layer_default` or an empty UUID. Import parsed raw UUID text into these fields, exporter independently repaired only Position, Symbol, and Layer XML spelling, project save embedded that export, and project load re-imported it. Group hierarchy used parent UUIDs and child references, while Perastage root UserData maps target exported object UUIDs. That arrangement allowed strict layer validation to reject project save and allowed a missing default Layer UUID to reach XML.

The Phase 4B recovery transaction now canonicalizes recoverable spellings, deterministically repairs missing/malformed/duplicate/key-field identities in stable sorted order, rebuilds map keys and fields together, rewrites group parent/child references, materializes default and inferred layers with canonical identities, and emits structured `mvr_identity_recovery` diagnostics. Valid canonical identities remain unchanged. Recovery mutates editable project state before strict export, making the replacement persist through project save/load and making a second transaction diagnostic-free and identity-idempotent. Export still keeps Position and Symbol/Symdef handling in their documented separate identity scopes.

### Local failure and verification evidence

The inherited first failures were already captured in this audit: `ProjectSupportUserDataRoundtrip` stopped at `ValidateLayers` with `Layer UUID is malformed`, while `MvrTrussRoundtripStructure` found a default Layer without the required canonical `uuid`. The former standard-strict fixture now uses canonical Layer and Support UUIDs; malformed identity is isolated in `MvrIdentityRecovery`. The latter is repaired by materializing a stable canonical default Layer before XML construction and always emitting one Layer ChildList.

During the initial Phase 4B commit the container lacked wxWidgets development packages, so only the GUI-independent tests ran locally. For the closure, the required Linux development packages were installed and CMake configured with `PERASTAGE_ENABLE_MVR_XCHANGE_MDNS=OFF` because the vcpkg mdns package is not available in this checkout. The expanded `MvrIdentityRecovery` passed 20 consecutive CTest repetitions; UUID, Layer, MVR Symbol identity, DMX conversion, all six Phase 4A controls, and the D1 policy controls passed. The two broad tests reproduced their documented later non-identity assertions, and `SaveLoadRoundtrip` retained its existing unrelated GDTF category-fixture failure. Complete cross-platform D2 evidence remains pending the follow-up CI run.


### Phase 4B CI follow-up and D2 classification

The reviewed Phase 4B head `52e46ffbf7ee1484f6b6f57ab42da0479072900f` was tested by authoritative Debug Tests run `30197537554`. Linux configured and built successfully and reported 163 total, 139 passed, 23 failed, and 1 skipped. Windows configured and built successfully and reported 165 total, 138 passed, and 27 failed. Toolchain and sccache stages passed on both platforms. The reduced macOS release-gate profile remained 6 passed of 6; it is not full macOS parity.

The complete Linux and Windows failure-name sets were unchanged from run `30196121317`. One newly registered test, `MvrIdentityRecovery`, passed on Linux and Windows, as did `UuidUtilsRfc4122`, `MvrSceneObjectSymbolIdentity`, all six Phase 4A controls, `WindowsDebugToolPreflight`, and `WxFilesystemPathBoundaries`. No test was hidden, skipped, disabled, relabeled, or weakened.

`ProjectSupportUserDataRoundtrip` previously stopped at `cfg.SaveProject(projectPath.string())` because Layer UUID validation rejected `layer1`. In run `30197537554`, save and Layer identity validation passed and the first failure moved to `userData != nullptr` at line 151. This proves the original identity blocker is repaired. Missing Support UserData in the project roundtrip is a separate contract explicitly deferred from Phase 4B.

`MvrTrussRoundtripStructure` previously stopped because the default exported Layer had no canonical `uuid`. In run `30197537554`, canonical default Layer identity and ChildList checks passed and the first failure moved to `!importedTruss.perastageAuxGdtfArchivePath.empty()` at line 398. This proves the original Layer identity blocker is repaired. Truss auxiliary GDTF archive-path restoration is a separate metadata contract explicitly deferred from Phase 4B.

D2 does not require these mixed-domain tests to become fully green. D2 requires the focused identity/reference matrix to pass repeatedly, the original identity-specific first failures to remain absent, and no new CI failing test name. The closure adds ambiguity-safe alias resolution and complete supported scene-reference rewriting.

### D2 merge record and Phase 4C baseline

PR #2222 was merged as `9ebdbf6b6cc2cecf00f7dde6c967017c09208e1a`; its final tested head was `fd938a2c3d10cd359ee803dea71daee9b011ca45`. Authoritative Debug Tests run `30199148987` reported Linux 163 total, 139 passed, 23 failed, and 1 skipped; Windows 165 total, 138 passed, and 27 failed; and the reduced macOS profile passed 6 of 6. No failing test name was added or removed relative to run `30197537554`. This is the final cross-platform evidence: D2 was reached.

The post-merge main-equivalent checkout is `2db1dd8ddb1023261a60736f4589c7da92458915`, version `1.5.11`. No remote is configured in this execution checkout, so fetch was unavailable. `git diff fd938a2c3d10cd359ee803dea71daee9b011ca45..2db1dd8` shows only `VERSION` as a post-head content change; the remaining difference is merge topology. Production and tests did not change after the verified head, so D2 controls did not require a baseline rerun.

The exact next failure group is canonical Perastage Support and Truss extension metadata roundtrips. `ProjectSupportUserDataRoundtrip` reaches canonical export and then asserts the obsolete direct `Support/UserData/Data/HoistInfo` shape. `MvrTrussRoundtripStructure` validates the root `TrussInfoMap` values and then reaches the missing auxiliary GDTF field assertion. `MvrSupportUserDataRoundtrip` and `MvrExporterCompliance` remain mixed controls whose first failures must be classified independently.

The pinned public MVR 1.6 XSD is `mvrdevelopment/tools` commit `16f9ff3624d3e715798a28b2c460579c55820853`, `mvr.xsd`, blob `b250b81a1a98f5dbeaf7eb55c54e21409d83f829`. It permits `GeneralSceneDescription/UserData`; Support and Truss do not define direct UserData. Strict Perastage output consequently uses only root `Data provider="Perastage" ver="1.0"` containing `HoistInfoMap` and `TrussInfoMap`. Direct object metadata is tolerant legacy input, and foreign-provider Data is not Perastage metadata.

Focused local classification confirmed `ProjectSupportUserDataRoundtrip` exported successfully before reaching its stale direct-object assertion. `MvrSupportUserDataRoundtrip` fails before canonical MVR export because its ad-hoc `fixture.gdtf` is not a valid ZIP and the canonicalizer rejects it; this is an invalid-fixture blocker rather than a metadata failure. The Truss structure test reaches its canonical root-map assertions before auxiliary-resource restoration. Unrelated GDTF/category repair remains deferred.

After the focused fixture and resource-contract corrections,
`ProjectSupportUserDataRoundtrip` and `MvrTrussRoundtripStructure` both pass
locally and pass `ctest --repeat until-fail:20`. D3 is not yet declared:
Windows and complete Debug Tests CI evidence, plus the requested exhaustive
malformed/duplicate metadata diagnostic matrix, remain the exact acceptance
blockers. The PR must remain focused and open until that evidence and coverage
are available; no unrelated repair is included.

### Phase 4C CI follow-up and D3 closure baseline

The reviewed Phase 4C head `5f6128f59205b730ad23d730404dae6dd8f6679f`
was tested by authoritative Debug Tests run `30203361878`. Linux reported 163
total, 141 passed, 21 failed, and 1 skipped; Windows reported 165 total, 140
passed, and 25 failed; the reduced macOS profile remained 6 passed of 6. Linux
and Windows each removed exactly `ProjectSupportUserDataRoundtrip` and
`MvrTrussRoundtripStructure`, with no added failing test name.

D3 remained pending at that head because `MvrSupportUserDataRoundtrip` still
used a plain-text non-GDTF fixture, metadata provider/version, duplicate,
numeric, and path rejection were not covered, and the Truss test did not prove
scene-owned auxiliary resource lifetime or safe re-export. The closure work
replaces the strict fixture with the shared canonical GDTF 1.2 builder, makes
supported root metadata first-wins over legacy input, exposes import
diagnostics, rejects non-finite numbers and unsafe or missing auxiliary paths,
prevents fallback substitution for an explicit missing auxiliary resource, and
exercises owned-resource re-export. Final D3 status remains contingent on the
closure test matrix and cross-platform CI evidence.

### Phase 4C final diagnostic-closure baseline

Authoritative Debug Tests run `30205630411` tested reviewed head
`0e64b4ba4d58554e38be6c1852bcbc813ee09c40`. Linux reported 163 total,
142 passed, 20 failed, and 1 skipped; Windows reported 165 total, 141 passed,
and 24 failed; the reduced macOS profile remained 6 of 6. The run removed
`MvrSupportUserDataRoundtrip` on Linux and Windows, added no failing test name,
and kept all three Support/Truss roundtrip tests passing.

D3 at that head remained pending only executable metadata diagnostic and
legacy-provider closure. `MvrPerastageMetadataRecovery` now exercises the
detailed `MvrImportResult` path and asserts structured codes for malformed,
duplicate, unknown, unsupported-version, legacy-version, numeric,
MotorFixtureUuid, and unsafe auxiliary metadata while verifying canonical-root
precedence and foreign-provider isolation. Final D3 declaration still requires
the new test and unchanged control matrix to pass in cross-platform CI.

At that diagnostic-closure head, the focused test and repository policy checks
passed locally. Archive-entry ambiguity and normal-flow diagnostic delivery
remained intentionally deferred to the next focused closure.

### Phase 4C archive-ambiguity closure baseline

Authoritative Debug Tests run `30207498081` tested reviewed head
`fa7eaee6381ff722d31ee6e262cc3fca8a69fa23`. Linux reported 164 total,
143 passed, 20 failed, and 1 skipped; Windows reported 166 total, 142 passed,
and 24 failed; the reduced macOS profile remained 6 of 6.
`MvrPerastageMetadataRecovery` passed on Linux and Windows, and the failure-name
delta from run `30205630411` was empty. All four focused metadata tests passed.

The exact remaining boundaries at that head were deterministic rejection of
duplicate or ASCII case-colliding ZIP entries and delivery of structured
diagnostics through common import overloads. Extraction now removes the first
colliding resource, skips later colliders, clears remaps, fails on ambiguous
scene XML, and retains collision diagnostics even on failure. Common project
and scene import entry points deliver final structured diagnostics once; the
detailed overload remains the non-logging structured source of truth. Final D3
still requires authoritative cross-platform CI for this closure commit.

## Phase 4D fixture category and GDTF reference roundtrips

### D3 merge evidence and focused baseline

PR #2223 merged as `d7a9afd3103e9806e51fa0ef973e8c2ab8a115cb` from tested head `d0b28a3924281164755279187bac565e638ab002`. Authoritative Debug Tests run `30209219695` reported Linux 164 total, 143 passed, 20 failed, and 1 skipped; Windows 166 total, 142 passed, and 24 failed; and the existing reduced macOS release-gate profile passed 6 of 6. The Linux and Windows failure-name sets were unchanged from the preceding Phase 4C run. This reduced macOS profile is not full-suite parity.

On required base `0fb8e12be42fe4c4bbf9162fd72016aa419f3747`, the independent Linux Debug reproductions were:

- `ctest --test-dir build/wsl-x64-debug -R '^GdtfFixtureCategoryFallback$' --output-on-failure --verbose`: failed at the former bare assertion on line 177. Case-level diagnostics identified `empty_attributes_unknown.gdtf`: expected `Unknown`, actual `Conventional`, reason `static conventional profile`. The parser counted unnamed XML containers as attribute definitions, a production defect. The case itself is an intentional incomplete, well-formed tolerant-recovery fixture, not a standard-strict publication.
- `ctest --test-dir build/wsl-x64-debug -R '^MvrFixtureCategoryRoundtrip$' --output-on-failure --verbose`: failed at `exporter.ExportToFile(...)` on the former line 147. Strict canonicalization rejected the plain-text `fixture.gdtf`; logs also diagnosed malformed Layer identity `layer1`. This was an invalid strict fixture with invalid identity/reference assumptions, plus missing Perastage contract documentation.

The same two test names failed in the cited Linux and Windows D3 run. Local evidence in this phase is Linux-only; Windows and macOS execution requires CI.

### Perastage category and reference contract

Fixture category is Perastage application metadata, not a normative GDTF or MVR enumeration. Strongest-to-weakest import precedence is: explicit fixture/project metadata (including accepted legacy per-fixture metadata); canonical root Perastage `FixtureTypeInfoMap` metadata keyed by portable GDTF/type identity; an existing dictionary category for the resolved fixture type; cached category for the same resolved type identity; GDTF structural and attribute signals; GDTF fixture-name signals; and `Unknown`. A present explicit scene value is never replaced by dictionary or inferred data. `Manual` records explicit provenance; `AutoFallback` records inference and retains its reason. New MVR output deduplicates root metadata by normalized fixture type/GDTF identity, prefers explicit `Manual` category on conflict, chooses visual color deterministically, and emits no per-Fixture extension nodes.

Absent metadata is resolved deterministically from normalized content and identity keys. Category reading remains tolerant: incomplete but well-formed ZIP/XML can supply safe signals without passing strict publication canonicalization. Empty or ambiguous evidence yields `Unknown`; missing `description.xml` yields `Unknown` with a diagnostic reason. Inference does not depend on the current working directory, filesystem filename casing, or dictionary serialization order.

Strict MVR output embeds canonical GDTF 1.2 ZIP resources exactly once under a portable archive name. Every Fixture `GDTFSpec` must resolve to that entry and every non-empty `GDTFMode` must name a mode in its referenced GDTF. Strict Layer and scene-object identities are RFC 4122 UUIDs. A legacy GDTFSpec/archive-name mismatch is tolerant compatibility input only: when one unambiguous canonical embedded name matches, import deterministically remaps the old reference to that embedded resource.

### Fixture classification and implementation result

`canonical_minimal.gdtf` is generated by the shared fixture builder and is the standard-strict control. The signal table (`moving_*`, static, LED, effects, media, laser, hoist, name-hint, and ambiguous cases) contains intentionally incomplete but well-formed ZIP/XML fixtures classified as `tolerant-recovery`; they test signal extraction rather than publication validity. `InferFromName("Tour Hazer")` is the name-only control. A missing archive is the malformed/missing-description control. Failure output now includes fixture/case name, expected category, actual category, inference reason, and policy classification.

The production parser now ignores unnamed XML containers when deciding whether an attribute definition exists. This restores `Unknown` for genuinely insufficient evidence without weakening tolerant reads. The strict MVR test now uses the shared canonical GDTF 1.2 builder, a real `ModeA`, RFC 4122 identities, an isolated temporary library/project context, exact archive-entry checks, GDTFSpec and GDTFMode resolution checks, explicit category/source/color preservation against weaker dictionary values, and a second canonical export/import cycle. The name-mismatch compatibility archive now embeds a real GDTF.

Final local commands and results are recorded after the verification matrix below. Cross-platform full-suite totals, failure-set delta, final tested branch SHA, and authoritative CI run ID remain pending until the branch is pushed and CI completes; D4A cannot be claimed from local Linux evidence alone. The next intended item after D4A is the separately scoped `GdtfReadServices` group, not rider work.

### Scoped dependent-control progression

After replacing the shared invalid fixtures, `SaveLoadRoundtrip` passed dictionary category propagation and strict project serialization. Its first failure advanced from `categoryPropagationA.has_value()` (missing `shared.gdtf`) and then `cfg.SaveProject(...)` (plain-text `orig.gdtf`) to the later assertion `scene2.fixtures.at("20000000-0000-4000-8000-000000000001").visualColorHex == "#445566"` at line 251. That broader project-restore visual metadata failure is recorded without weakening the assertion.

`MvrExporterCompliance` advanced from `exporter.ExportToFile(...)` failing canonicalization of plain-text `A/Same.gdtf` to strict MVR validation of Truss child order: `Truss uuid '12345678-1234-4234-9234-123456789abc' writes child CustomIdType after CustomId, violating MVR 1.6 order`. Truss serialization order is unrelated to fixture category/GDTF reference validity and is deferred. The test remains unchanged beyond replacing its invalid strict GDTF resources.

### Phase 4D local verification result

Linux Debug configure and focused builds passed with `cmake --preset wsl-x64-debug -DPERASTAGE_ENABLE_MVR_XCHANGE_MDNS=OFF` and `cmake --build build/wsl-x64-debug --target gdtf_fixture_category_test mvr_fixture_category_roundtrip_test -j3`. The mdns backend was disabled only because this system-package environment has no vcpkg `mdns` package.

- `ctest --test-dir build/wsl-x64-debug -R '^(GdtfFixtureCategoryFallback|MvrFixtureCategoryRoundtrip)$' --repeat until-fail:20 --output-on-failure`: 2 of 2 passed through 20 repetitions each.
- `ctest --test-dir build/wsl-x64-debug -R '^(SaveLoadRoundtrip|MvrExporterCompliance)$' --output-on-failure`: both advanced beyond their invalid GDTF blockers, then failed at the recorded visual-color and Truss-order assertions. Neither later assertion was changed.
- The requested 14-test D1-D3 regression expression passed 14 of 14 locally, including both platform policy scripts registered as CTest tests.
- `tests/check_perastage_tree_modules.sh` and `tests/check_no_configmanager_get_in_gui.sh`: passed.
- `PERASTAGE_TEST_PYTHON="$(command -v python3)" python3 tests/check_wx_filesystem_path_boundaries.py`, `python3 tests/check_bash_test_registration.py`, and `bash tests/check_ci_cmake_language_policy.sh`: passed with `PERASTAGE_TEST_PYTHON` exported for the harness contract.
- `git diff --check`: passed.

No Linux full registered-suite run, Windows run, or macOS reduced release-gate run was available in this Linux container. Consequently there is no new CI run ID or authoritative full-suite failure-set delta, and D4A is not declared. The final branch commit SHA and CI run ID must be added to the PR evidence after push and CI; the prior authoritative baseline remains run `30209219695` at tested head `d0b28a3924281164755279187bac565e638ab002`.

### D4A merge evidence and Phase 4E baseline

PR #2224 merged reviewed branch `codex/fix-gdtf-reference-roundtrips` as merge
commit `240edbef812837398d843869b05604e565d9feba`. Its base was
`0fb8e12be42fe4c4bbf9162fd72016aa419f3747`, reviewed head was
`1ecafba0ceb0a97db254b30b0f303c7678a440ac`, and authoritative Debug Tests run
`30239020200` reported Linux 164 total, 145 passed, 18 failed, and 1 skipped;
Windows 166 total, 144 passed, and 22 failed; and the reduced macOS release gate
6 passed of 6. Configure, build, toolchain, vcpkg, and sccache stages passed on
Linux and Windows. The reduced macOS profile is not full macOS parity.

Relative to D3 run `30209219695`, Linux and Windows each removed exactly
`GdtfFixtureCategoryFallback` and `MvrFixtureCategoryRoundtrip`, with no added
failing test name. Both Phase 4D primary tests and all retained D1-D3 focused
controls passed on Linux and Windows. D4A was reached and PR #2224 was safe to
merge.

The Phase 4E branch `codex/repair-gdtf-read-services-unicode-paths` starts from
post-merge main-equivalent SHA
`d3861f141f92d7a16868d251c706c54f25ea0b6f`, version `1.5.13`. No remote is
configured in this execution checkout, so `git fetch --all --prune` had no
remote updates to retrieve. Local ancestry confirms that this base contains the
reviewed Phase 4D head and merge commit above.

Before production changes, Linux Debug reproduced `GdtfReadServices` at the
malformed-byte subcase: invalid filename bytes with ZIP UTF-8 bit 11 set still
left `read.Success()` true at line 492. `GdtfFixtureInsertionPreparation` passed
on Linux. Authoritative Windows run `30239020200` independently records the
platform-specific outer-path failures: `GdtfReadServices` could not access
`metadata_ñ_测试.gdtf`, and `GdtfFixtureInsertionPreparation` failed to prepare
`Perastage_ñ_fixture.gdtf`. The required contract is documented in
`technical-notes/gdtf_unicode_zip_filename_compatibility.md` before application
behavior changes.

### Phase 4E local repair evidence

The Linux malformed-byte failure was primarily a test-fixture construction
defect: the arbitrary byte search compared signed `char` UTF-8 bytes with
`unsigned char` archive bytes and therefore never changed the intended Unicode
name. The fixture now parses ZIP local and central headers, matches the exact
raw identity byte-for-byte, changes only its first name byte and bit 11 fields,
and proves exactly one local and one central record were patched. Cases place
the invalid entry before and after `description.xml`, among multiple entries,
on `description.xml` itself, and beside a valid Unicode resource.

Production now validates every raw central-directory identity before asking
wxWidgets to stream payloads. An explicitly UTF-8 but undecodable identity
therefore fails with `FilenameDecodeFailed` before a skipped entry can shift raw
metadata onto another payload. Archive, resource, and extraction streams use
the shared native `WxPathUtils` conversion. Metadata summary loading also has a
native filesystem-path overload, and the two test ZIP writers no longer route
native paths through narrow `generic_string()` text. These path conversions
classify the two Windows primaries as platform-specific wx/filesystem boundary
defects.

Local Linux Debug results:

- The two primary tests passed 20 consecutive repetitions each.
- Eight registered scoped controls ran; seven passed. The requested
  `GdtfModeChannelPresenter` name is not registered in this checkout.
  `TrussPathEncodingRegression` advanced to its unrelated broad persisted
  last-project-path assertion at line 120 and remains visible and deferred.
- Both mandatory module checks, the wx filesystem scanner, Bash registration
  check, CI CMake language policy, and `git diff --check` passed. The CMake
  language check required `PERASTAGE_TEST_PYTHON` to be supplied by the harness,
  as documented by that policy test.

No test was skipped, disabled, hidden, renamed, relabeled, removed, or weakened.
Full Linux/Windows Debug Tests and reduced macOS release-gate CI artifacts are
not available because this checkout has no configured remote or CI dispatch
surface. Consequently there is no Phase 4E CI run ID, authoritative platform
total, or verified failure-set delta, and D4B is not declared. The branch must
continue through complete CI before it can be recommended for merge.

### Phase 4F canonical object order and project fixture metadata

Phase 4F starts from the latest main-equivalent checkout SHA
`843b995ad2551d971f22fea184e94f240cb29829`, version `1.5.15`, on the new branch
`codex/repair-mvr-order-and-project-metadata-roundtrips`. PR #2225 is present as
merge commit `978afeb6c490c44be7cd98f86ef2b321a8aac077`; its reviewed head
`0f8c0523c9558c4f28498557dcb1f9e51bc487cd` has no content difference from the
merge result. Later main changes comprise version bumps and the non-overlapping
issue-intake PR #2226. Debug Tests run `30241403702` remains the authoritative
pre-Phase-4F baseline: Linux 164 total, 146 passed, 17 failed, 1 skipped;
Windows 166 total, 146 passed, 20 failed; reduced macOS 6 of 6 passed.

The authoritative MVR 1.6 schema is `mvrdevelopment/tools/mvr.xsd` at commit
`16f9ff3624d3e715798a28b2c460579c55820853` (Git blob
`b250b81a1a98f5dbeaf7eb55c54e21409d83f829`, 28,286 bytes, SHA-256
`c6dadedf91d9bd93148f2fdb3843cfca9c8f8ec0b86f035cbb4110def74af5ef`).
Its `Truss` sequence places `CustomIdType` immediately before `CustomId`, as do
Fixture, Support, VideoScreen, and Projector. SceneObject differs and places
`CustomId` before `CustomIdType`. The Truss writer, not the strict validator,
was wrong: it wrote `CustomId` first. Serialization now follows the verified
Truss sequence and the existing schema-specific validation table checks it.

The project color loss occurred before archive publication: standalone-style
scene serialization collapsed fixture colors into type-keyed
`FixtureTypeInfoMap`, which cannot represent different values for fixtures that
share one GDTF/type identity. Project saves now explicitly add root Perastage
`ProjectFixtureMetadataMap schemaVersion="1.0"` entries keyed by canonical
fixture UUID and sorted deterministically. Only `ProjectRestore` reads the map;
normal MVR import remains unchanged. Valid instance values override type-level
colors. Malformed UUIDs, duplicate UUIDs, unsupported versions, and unknown
fixture UUIDs are ignored with structured diagnostics; first valid duplicate
wins, and foreign-provider data is ignored.

Authoritative follow-up run `30246813381` at reviewed head
`9e48e4acdc1202bf6f028498f25a7383f74e0fc9` completed configure, build,
toolchain, vcpkg, and sccache stages on Linux and Windows. Linux reported 164
total, 146 passed, 17 failed, and 1 skipped; Windows reported 166 total, 146
passed, and 20 failed; the reduced macOS release gate remained 6 of 6. Failure
names were unchanged from run `30241403702`: neither primary name was removed
because each advanced to a later assertion.

Closure preserves meaningful `FixtureID` text when only a duplicate
`FixtureIDNumeric` is repaired; empty or numeric-fallback text follows the new
globally unique positive numeric value. Case-collision coverage now verifies
the two fixtures' canonical GDTFSpec references, case-folded uniqueness,
one-to-one archive resources, expected FixtureType identities, and repeated
export stability rather than obsolete source filenames. Project metadata
recovery now reports `invalid_project_fixture_visual_color`, and focused
coverage exercises ProjectRestore-only application, external/merge isolation,
duplicates, malformed and unknown UUIDs, unsupported versions, foreign data,
and invalid colors.

Run `30249758520` at reviewed head
`2cd6ebdebeeec03c92f958f4e80544fe3780c4ff` confirms the two monolithic tests
now stop only at independent retained assertions: `SaveLoadRoundtrip` at the
Viewer2D fixture-label override assertion on line 353 and
`MvrExporterCompliance` at the primitive sphere matrix assertion on line 989.
Linux retained 17 failing names and Windows retained 20, with no added failure
name; the reduced macOS profile remained 6 of 6.

Independent Phase 4F evidence is registered as
`MvrProjectFixtureMetadataContracts`. Its minimal scene verifies standalone and
project export boundaries, two project cycles, per-instance colors (including
an explicit empty override), FixtureID text/numeric repair without editable
scene mutation, deterministic metadata and numeric repair, and semantic
case-collision GDTF resources. The focused test exposed and drove one bounded
production correction: project saves now represent an explicit empty color so
weaker type metadata cannot repopulate it. Locally the focused test passed five
consecutive repetitions, and all nine requested registered scoped controls
passed. Final cross-platform CI for the new test is still required before D4C
can be declared. No test, assertion, registration, label, timeout, or coverage
was hidden or weakened.

PR #2227 closed Phase 4/D4C at reviewed head
`96b32c7902059c3c2562fa4ad0fd063dc0bd13bb`: authoritative run `30253286128`
passed `MvrProjectFixtureMetadataContracts` on Linux and Windows, introduced no
new failure name, and kept the reduced macOS profile at 6 of 6. D4C was reached
and PR #2227 was approved for merge.

Phase 5A classifies `RiderComments` and `RiderFilterPreview` as stale snapshot
expectations. The production formatter already emits consistent compact blocks,
omits removed comments rather than materializing empty blocks, normalizes CRLF
input, and is idempotent; focused tests now encode those byte-level invariants.


Authoritative run `30270379861` at reviewed head
`b1351fcd989cd753bfeaedb1106375cbeddf7595` confirms that both original Phase 5B
blockers advanced. `RiderHoistImport` now first stops at the independent Phase 5D
`filteredBackdropTrussHasModelInfo` assertion, while `RiderPipeImport` now first
stops at the independent Phase 5C fixture-coordinate Y assertion. The registered
`RiderRiggingTargetNormalizationContracts` test isolates hoist alias and pipe LX
target contracts from those later failures; the local focused run and five repeat
runs pass. E2 remains pending cross-platform focused-test evidence.

PR #2229 closed Phase 5B/E2 at reviewed head
`8d5b2f6f01f6eaeca89aac8388c25835664e52f5`. Authoritative run
`30274849325` passed `RiderRiggingTargetNormalizationContracts` on Linux and
Windows, made `RiderPipeImport` green, introduced no new failure name, and left
`RiderHoistImport` at its deferred Phase 5D backdrop-resource assertion. E2 was
reached and PR #2229 was approved for merge.

PR #2230 closed Phase 5C/E3 at reviewed head
`4c1f88e59bb67cde545a208c64f061f2f53ab7ac`. Authoritative run
`30280209293` made `RiderImportLinearOrder`, `RiderLedScreenObject`, and
`RiderLxSidesImport` green on Linux and Windows, introduced no new failure
name, and kept macOS at 6 of 6. E3 was reached and PR #2230 was approved for
merge.
