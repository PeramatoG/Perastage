# ORG-039 Local Build Workflow Validation

## Scope and current status

This is an execution-evidence record, not a new build specification. The
canonical instructions remain in the [Build and Dependency Guide](build.md).
CI is context only and does not replace native local execution.

- **Merged `main` SHA:** `bfada84e35db41bcd1bc4deb4bd6fbaa24396e76`
- **GitHub-reachable Windows test head:** `f07b04d2fc4e0d5c164250730d202e7673c8bb29`
- **GitHub-reachable WSL test head:** `2375c1df439129cb4c01c04a43aea52d1cfa0a1e`
- **GitHub-reachable final validation head:** `ed8c4063fd6a60e69b901ea670c7b76d1edc8b71`
- **Original Linux execution identifier:** `2483afe549e8a47b8ad153f81780e6c784f3e8ec` (local historical identifier; the reproducible evidence reference is the published PR head above)
- **Initial validation date:** 2026-09-07
- **Windows and WSL corrective follow-up date:** 2026-09-08
- **Precondition:** the tested commit contains merge `056f3f4`, PR #2339
  (**ORG-038: complete repository-organization regression audit**).

All four supported environments now have complete clean-checkout local
validation evidence. ORG-039 remains unchecked until this PR is merged to the
default branch, as required by the immutable checklist contract. ORG-040 and
ORG-041 have not started.

## Validation matrix

| Platform | Exact environment | Tested commit SHA | Clean checkout and prerequisites | Setup launcher | Debug configure / build / CTest | Release configure / build | Stage and resources | Canonical presets | Local override | Final status |
|---|---|---|---|---|---|---|---|---|---|---|
| Windows x64 native | Native Windows x64; external classic vcpkg at `C:\vcpkg` | `f07b04d2fc4e0d5c164250730d202e7673c8bb29` | PASS: clean checkout, external classic vcpkg, no generated local configuration | PASS | PASS: 249/249 | PASS | PASS: staged resources verified | PASS | PASS: ignored temporary user preset removed | **PASS** |
| macOS Apple Silicon | Native arm64, macOS 26.5.1 (25F80), Command Line Tools, CMake 4.2.3, Ninja 1.13.2 | `ed8c4063fd6a60e69b901ea670c7b76d1edc8b71` | PASS: clean checkout, Homebrew gettext/ripgrep, external classic vcpkg with wxWidgets secretstore | NOT APPLICABLE: no documented macOS setup launcher | PASS / PASS / PASS: 248/248 | PASS / PASS: 371/371 | PASS: arm64 application and staged resources verified | PASS | PASS: ignored inherited preset configured and removed | **PASS** |
| Native Linux x64 | Ubuntu 24.04.4 LTS, Linux 6.18.35 x86_64, GCC 13.3.0, CMake 3.28.3, Ninja 1.11.1 | `f07b04d2fc4e0d5c164250730d202e7673c8bb29` (published equivalent of the executed source state) | PASS: detached `/tmp` clone, empty initial porcelain status, no initial build/output/user preset; Ubuntu development packages and external vcpkg `mdns:x64-linux` | PASS: root `setup.sh` invoked by absolute path from `/tmp` with `Debug --skip-deps --skip-build` | PASS / PASS / PASS: 247 total, 245 passed, 0 failed, 2 expected environment-dependent skips | PASS / PASS | PASS: generated dummy fixture, bundled library, catalogs, resources, help, and licenses | PASS | PASS: ignored inherited preset listed and configured, then removed | **PASS** |
| WSL x64 | WSL2 Ubuntu 24.04.1 LTS x86_64; checkout under `/home/peramato/Perastage-ORG039-WSL` | `425afc43aa81082a46ebb7ca5f1d0a465c2a3daf` | PASS: clean checkout in WSL Linux filesystem; bootstrap installed dependencies from an initially CMake-less environment; external baseline `mdns:x64-linux` | PASS | PASS | PASS | PASS | PASS | PASS | **PASS** |

The checked-in names match every requested canonical family: Windows
`win-x64-debug-ninja`, `win-x64-release-ninja`, `win-debug-build-ninja`,
`win-release-build-ninja`, and `win-release-stage-ninja`; macOS
`mac-arm64-debug`, `mac-arm64-release`, `mac-debug-build`, and
`mac-release-build`; Linux/WSL `wsl-x64-debug`, `wsl-x64-release`,
`wsl-debug-build`, `wsl-release-build`, and `wsl-release-stage`.

## Native Linux evidence

### Clean checkout and externally supplied prerequisites

```bash
rm -rf /tmp/perastage-org039-linux
git clone --no-hardlinks /workspace/Perastage /tmp/perastage-org039-linux
git -C /tmp/perastage-org039-linux checkout --detach 2483afe549e8a47b8ad153f81780e6c784f3e8ec
git -C /tmp/perastage-org039-linux rev-parse HEAD
git -C /tmp/perastage-org039-linux status --porcelain=v1
test ! -e /tmp/perastage-org039-linux/build
test ! -e /tmp/perastage-org039-linux/CMakeUserPresets.json
git -C /tmp/perastage-org039-linux ls-files CMakeUserPresets.json
```

The SHA matched; both Git outputs were empty; both absence checks passed. No
Perastage build tree was reused. System prerequisites were installed externally:

```bash
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential cmake \
  ninja-build git curl pkg-config libgl1-mesa-dev libglu1-mesa-dev \
  libglew-dev libcurl4-openssl-dev libtinyxml2-dev libpodofo-dev zlib1g-dev \
  libwxgtk3.2-dev gettext locales libmeshoptimizer-dev libbackward-cpp-dev libnanovg-dev
git clone --filter=blob:none https://github.com/microsoft/vcpkg.git /tmp/vcpkg-org039
git -C /tmp/vcpkg-org039 checkout 0878b5224d4a4968940ee296a2e7fae2d3b62983
/tmp/vcpkg-org039/bootstrap-vcpkg.sh -disableMetrics
/tmp/vcpkg-org039/vcpkg install mdns:x64-linux \
  --x-install-root=/tmp/vcpkg-installed-org039
```

The vcpkg commit is the repository manifest baseline. Only the required mDNS
CMake package came from this external installation; the canonical preset still
used the documented Linux system-package model. Initial configure attempts failed first for absent `meshoptimizer`, then for absent
`mdns`. Supplying those external prerequisites exposed a package-integration
regression: the vcpkg `mdns::mdns` target did not export the directory containing
`mdns.h`. Dependency discovery now locates that required header explicitly and
adds it to the imported target. The Ubuntu launcher dependency list was also
completed for meshoptimizer, NanoVG, backward-cpp, gettext, and the locales
required by the complete test suite.

### Setup launcher and canonical commands

The launcher worked from outside the repository and delegated to the Linux
implementation, configured `wsl-x64-debug`, and honored both documented flags:

```bash
cd /tmp
CMAKE_PREFIX_PATH=/tmp/vcpkg-installed-org039/x64-linux \
  /tmp/perastage-org039-linux/setup.sh Debug --skip-deps --skip-build
```

The canonical workflow was then exercised without test filtering:

```bash
cd /tmp/perastage-org039-linux
CMAKE_PREFIX_PATH=/tmp/vcpkg-installed-org039/x64-linux cmake --preset wsl-x64-debug
CMAKE_PREFIX_PATH=/tmp/vcpkg-installed-org039/x64-linux cmake --build --preset wsl-debug-build
ctest --test-dir build/wsl-x64-debug --output-on-failure
CMAKE_PREFIX_PATH=/tmp/vcpkg-installed-org039/x64-linux cmake --preset wsl-x64-release
CMAKE_PREFIX_PATH=/tmp/vcpkg-installed-org039/x64-linux cmake --build --preset wsl-release-build
CMAKE_PREFIX_PATH=/tmp/vcpkg-installed-org039/x64-linux cmake --build --preset wsl-release-stage
```

The preset's `/mnt/c` ignore values appeared in configure output and did not
block native-Linux discovery.

CTest reported 247 tests: 245 passed, 0 failed, and 2 were skipped.
`EditableFocusUtils` was skipped in the headless environment and
`CredentialStoreNativeRoundTrip` was skipped because the system wxWidgets build
did not expose native secure storage; both are expected environment-dependent
skips. An initial CTest run also identified missing host locales; generating
`es_ES.UTF-8` and `zh_CN.UTF-8` resolved the localization integration failure.
The first full run also showed that two tests wrote into the source tree.
The Git Bash resolver now removes its temporary probe and library-using tests
operate on a build-tree copy. The final full run passed and left tracked source
content unchanged.

### Generated and staged resources

```bash
test -x out/install/Release/Perastage
test -f 'out/install/Release/library/fixtures/Dummy 1ch.gdtf'
test -d out/install/Release/library
test -f out/install/Release/resources/locale/es/LC_MESSAGES/perastage.mo
test -f out/install/Release/resources/locale/zh_CN/LC_MESSAGES/perastage.mo
test -d out/install/Release/resources
test -f out/install/Release/help.md
test -f out/install/Release/LICENSE.txt
test -f out/install/Release/THIRD_PARTY_LICENSES.md
test -d out/install/Release/licenses
```

All checks passed, covering the current runtime-staging and install ownership.

## Local user preset

Only the temporary checkout received this schema-v3 file:

```json
{
  "version": 3,
  "configurePresets": [{
    "name": "org039-linux-debug",
    "inherits": "wsl-x64-debug",
    "binaryDir": "${sourceDir}/build/org039-linux-debug",
    "environment": {
      "CMAKE_PREFIX_PATH": "/tmp/vcpkg-installed-org039/x64-linux"
    }
  }]
}
```

```bash
git check-ignore -v CMakeUserPresets.json
git ls-files --error-unmatch CMakeUserPresets.json
cmake --list-presets
cmake --preset org039-linux-debug
git diff --exit-code -- CMakePresets.json
rm CMakeUserPresets.json
rm -rf build/org039-linux-debug
git ls-files CMakeUserPresets.json
git status --porcelain=v1 --untracked-files=all
```

Git identified the ignore rule and confirmed the file was never tracked. CMake
listed and configured the inherited preset, the shared preset remained
unchanged, and the temporary file was removed.

## Windows external validation follow-up

Native Windows x64 validation completed successfully at GitHub-reachable head
`f07b04d2fc4e0d5c164250730d202e7673c8bb29`. The stable setup launcher and
canonical Debug configure passed, the Debug build completed, and the full CTest
suite passed 249 of 249 tests. The canonical Release configure/build and stage
passed; generated and copied resources were verified. The ignored
`CMakeUserPresets.json` override worked and was removed, the checkout remained
clean, and no repository-local `vcpkg_installed` tree was created. CI Debug
Tests run #467 for the same head also completed successfully. This local result,
not CI alone, promotes the Windows row to PASS.

## WSL external validation follow-up

A genuinely clean WSL2 Ubuntu 24.04.1 LTS x86_64 environment checked out the
GitHub-reachable head `2375c1df439129cb4c01c04a43aea52d1cfa0a1e` under
`/home/peramato/Perastage-ORG039-WSL`, inside the Linux filesystem rather than
`/mnt/c`. CMake was not preinstalled. The corrected
`./setup.sh Debug --skip-build` invocation installed the Ubuntu dependencies
and test locales before the CMake preflight. External `mdns:x64-linux` was then
installed from repository vcpkg baseline
`0878b5224d4a4968940ee296a2e7fae2d3b62983` and exposed through
`CMAKE_PREFIX_PATH=/home/peramato/Perastage-ORG039-vcpkg-installed/x64-linux`.
The subsequent `./setup.sh Debug --skip-deps --skip-build` configure passed,
and `cmake --build --preset wsl-debug-build` completed all 2475 build steps.

The first full `ctest --test-dir build/wsl-x64-debug --output-on-failure` run
reported 249 tests, 36 failures, and one expected environment-dependent
`CredentialStoreNativeRoundTrip` skip. Thirty-four failures were caused by
missing `rg`. The other two were the Windows-only
`PowerShellNativeCaptureWindowsPowerShell` and
`PowerShellNativeCapturePowerShell7` tests, which CMake incorrectly registered
after discovering Windows executables inherited through the WSL `PATH`.

The focused correction adds ripgrep to both supported Linux package-manager
dependency sets and retains `--skip-deps` as a strict no-install path. It also
gates PowerShell native-capture discovery and registration on a Windows CMake
host while preserving both tests on Windows. Deterministic entry-point and
registration policy checks cover these contracts. The external mDNS
requirement, canonical preset selection, and all cross-platform policy tests
remain unchanged.

The corrected workflow was subsequently rerun through setup, Debug
configure/build/CTest, Release configure/build/stage, resource checks, the
ignored local override, and final checkout cleanliness at
`425afc43aa81082a46ebb7ca5f1d0a465c2a3daf`. All required checks passed, so the
WSL row is PASS based on local execution rather than CI.

## macOS Apple Silicon external validation follow-up

A clean native Apple Silicon checkout at
`/Users/arenasaudio/Perastage-ORG039-macOS` completed final validation at
GitHub-reachable head `ed8c4063fd6a60e69b901ea670c7b76d1edc8b71` on macOS
26.5.1 (build 25F80). The environment used arm64 Command Line Tools from
`/Library/Developer/CommandLineTools`, CMake 4.2.3, Ninja 1.13.2, Git 2.52.0,
Homebrew 6.0.12, gettext, ripgrep 15.2.0, and the external
`/Users/arenasaudio/vcpkg` checkout at manifest baseline
`0878b5224d4a4968940ee296a2e7fae2d3b62983`. That checkout supplied wxWidgets
3.3.1#1 with secretstore support.

### Debug and tests

```bash
export VCPKG_ROOT="$HOME/vcpkg"
cmake --preset mac-arm64-debug
cmake --build --preset mac-debug-build
ctest --test-dir build/mac-arm64-debug \
  -R '^ReleaseGatePolicyPortability$' --output-on-failure
ctest --test-dir build/mac-arm64-debug --output-on-failure
```

Debug configure and build passed. The focused corrected portability test passed
1 of 1. The complete suite then passed 248 of 248 tests in 23.35 seconds with
no failures; `CredentialStoreNativeRoundTrip` passed.

Historically, the first macOS run at
`425afc43aa81082a46ebb7ca5f1d0a465c2a3daf` passed 247 of 248 tests and exposed
only `ReleaseGatePolicyPortability`. The restricted-PATH wrapper correction at
`40e9e4ce54d026aa92052030aec12c853c37016d` fixed the copied macOS `dirname`
behavior. Windows CI then exposed equivalent Git Bash drive paths written in
different forms; the final correction at the validated head canonicalized both
directories while retaining the restricted-PATH regression coverage.

### Release, staging, and resources

```bash
rm -rf build/mac-arm64-release
export VCPKG_ROOT="$HOME/vcpkg"
cmake --preset mac-arm64-release
cmake --build --preset mac-release-build
cmake --build build/mac-arm64-release --target perastage_stage
```

Release configure and all 371 build steps passed, linking the arm64 Mach-O
executable at `Perastage.app/Contents/MacOS/Perastage`. Staging passed to
`out/install/Release/Perastage.app`. Verification covered `Info.plist`, the
executable, Spanish and Simplified Chinese gettext catalogs, the logo, icons,
font, generated `Dummy 1ch.gdtf`, fixture and truss dictionaries, `LICENSE.txt`,
`THIRD_PARTY_LICENSES.md`, and `help.md`. The validated macOS contract retains
runtime data below `Contents/Resources/resources/` and catalogs below
`Contents/Resources/locale/`.

### Local override and cleanliness

A temporary ignored schema-v3 `CMakeUserPresets.json` defined
`org039-mac-debug`, inherited `mac-arm64-debug`, and used
`${sourceDir}/build/org039-mac-debug`. Configure passed, and its cache confirmed
Debug, `arm64`, and `arm64-osx` for `CMAKE_BUILD_TYPE`,
`CMAKE_OSX_ARCHITECTURES`, and `VCPKG_TARGET_TRIPLET`. Git confirmed the user
preset was ignored and never tracked, shared `CMakePresets.json` was unchanged,
and no repository-root `vcpkg_installed` existed. The temporary preset and build
directory were removed, leaving the checkout clean.

All required local rows are PASS based on native clean-checkout execution. CI
Debug Tests run #471 also completed successfully at
`ed8c4063fd6a60e69b901ea670c7b76d1edc8b71`; this is supporting context and was
not substituted for local evidence. This documentation-only closure creates a
new head whose CI result must be recorded separately after GitHub completes it.
ORG-039 remains unchecked until merge. ORG-040 release/installer validation and
ORG-041 checklist finalization were not started.
