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

## Phase 1 CI baseline update for PR #2204

Branch base SHA: `ec6dee42371f51dc02520e3eb9fc4bea4d0daeca`.
Current reviewed head before this follow-up: `4732460bc10bd2e312bb2714d8a6e82f617c5a01`.
Current final head after this follow-up: recorded in PR #2204 after the final commit for this task.
CI Debug Tests run inspected: `29961799720` (`https://github.com/PeramatoG/Perastage/actions/runs/29961799720`).

As of the API inspection performed from this workspace on 2026-07-22, run `29961799720` had not completed. The workflow run status was `in_progress`, with `linux-debug`, `windows-debug`, and `macos-debug` also still `in_progress`; only `resolve-source` had completed successfully. Therefore no completed configure/build/CTest baseline, pass/fail/skip/not-run counts, or current failing-test list can be recorded from that run yet. This must not be represented as a test failure because the jobs had not reached a completed result.

| Platform | Configure result | Build result | CTest result | Passed | Failed | Skipped | Not run | Current failing tests | Log or artifact reference |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| Linux | Pending in run `29961799720` | Pending in run `29961799720` | Pending in run `29961799720` | Not available | Not available | Not available | Not available | Not available until completion | `ci-linux-debug-ctest-inventory`; `ci-linux-debug-diagnostics` on failure |
| Windows | Pending in run `29961799720` | Pending in run `29961799720` | Pending in run `29961799720` | Not available | Not available | Not available | Not available | Not available until completion | `ci-windows-debug-ctest-inventory`; `ci-windows-debug-diagnostics` on failure |
| macOS | Pending in run `29961799720` | Pending in run `29961799720` | Pending in run `29961799720` | Not available | Not available | Not available | Not available | Not available until completion | `ci-macos-debug-ctest-inventory`; `ci-macos-debug-diagnostics` on failure |

### Focused harness validation status

- Local Linux restricted-path validation passes for `ReleaseGatePolicyPortability`.
- Cross-runner Linux, Windows Git Bash, and macOS validation remains pending because run `29961799720` had not completed when inspected.
- `UnresolvedPythonInvocations` passes locally and is registered for CI.
- Windows Microsoft Store Python launcher avoidance remains pending runner confirmation; the policy test and existing `PythonResolvedInterpreterPolicy` are the focused checks intended to prove it.
- The portability harness directly runs the release-gate scripts from the repository root and an unrelated temporary working directory with a restricted PATH.

### Diagnostics baseline

- Linux CTest diagnostics are configured to preserve JUnit output, the full CTest log, `LastTestsFailed.log`, and the concise failure CSV in `out/ci-logs` and the build `Testing/Temporary` tree.
- Windows CTest diagnostics are configured to preserve JUnit output, the full CTest log, `LastTestsFailed.log`, and the concise failure CSV in `out/ci-logs` and the build `Testing/Temporary` tree.
- macOS now preserves CTest JSON inventory plus both the wrapper log and CTest `--output-log` file, and it writes JUnit output for release-gate tests so the same failure-summary path can include structured test results.
- Linux, Windows, and macOS now generate `ctest --show-only=json-v1` inventory files under `out/ci-logs` after a successful configure/build and before running tests. These generated inventory files are uploaded as dedicated CI diagnostic artifacts and are not committed to the repository.

### Baseline boundary

This follow-up remains at Safe Merge Point A until a completed CI run confirms the focused harness checks on all three platforms. No production code, product assertions, golden outputs, GDTF/MVR behavior, or rider expectations were changed.
