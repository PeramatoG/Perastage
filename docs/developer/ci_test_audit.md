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
