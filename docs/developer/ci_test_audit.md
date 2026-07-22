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
