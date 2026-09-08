# ORG-039 Local Build Workflow Validation

## Scope and current status

This is an execution-evidence record, not a new build specification. The
canonical instructions remain in the [Build and Dependency Guide](build.md).
CI is context only and does not replace native local execution.

- **Merged `main` SHA:** `bfada84e35db41bcd1bc4deb4bd6fbaa24396e76`
- **GitHub-reachable Windows rerun head:** `ee201dc5758df89c0a371e2060c339359181586d`
- **Original Linux execution identifier:** `2483afe549e8a47b8ad153f81780e6c784f3e8ec` (local historical identifier; the reproducible evidence reference is the published PR head above)
- **Initial validation date:** 2026-09-07
- **Windows corrective follow-up date:** 2026-09-08
- **Precondition:** the tested commit contains merge `056f3f4`, PR #2339
  (**ORG-038: complete repository-organization regression audit**).

**ORG-039 is not yet complete; external local validation remains required.**
The Windows rerun passed setup, configure, and the Debug build, then exposed
missing Debug test-tool documentation/preflight and two test portability issues. Apple Silicon macOS and WSL x64 were
unavailable. All three remain pending rather than inferred from GitHub Actions.
Native Linux was exercised
in its own clean checkout.

## Validation matrix

| Platform | Exact environment | Tested commit SHA | Clean checkout and prerequisites | Setup launcher | Debug configure / build / CTest | Release configure / build | Stage and resources | Canonical presets | Local override | Final status |
|---|---|---|---|---|---|---|---|---|---|---|
| Windows x64 native | Native Windows x64; external classic vcpkg at `C:\vcpkg` | `ee201dc5758df89c0a371e2060c339359181586d` | PASS: external clean checkout and classic-vcpkg prerequisites | PASS: Debug clean setup/configure and secure-store probe | Configure PASS; build PASS (2476/2476); CTest FAIL (249 total: 213 passed, 36 failed) | Not run | Not run | Debug canonical configure/build PASS | Not run | **PENDING EXTERNAL LOCAL VALIDATION** |
| macOS Apple Silicon | Native Apple Silicon macOS unavailable | `c7fb7121434e3c499ee250c7b5afa732e8365040` target, not executed | Not run against target `c7fb7121434e3c499ee250c7b5afa732e8365040` | No documented launcher | Not run | Not run | Not run | Names inspected only | Not run | **PENDING EXTERNAL LOCAL VALIDATION** |
| Native Linux x64 | Ubuntu 24.04.4 LTS, Linux 6.18.35 x86_64, GCC 13.3.0, CMake 3.28.3, Ninja 1.11.1 | `c7fb7121434e3c499ee250c7b5afa732e8365040` (published equivalent of the executed source state) | PASS: detached `/tmp` clone, empty initial porcelain status, no initial build/output/user preset; Ubuntu development packages and external vcpkg `mdns:x64-linux` | PASS: root `setup.sh` invoked by absolute path from `/tmp` with `Debug --skip-deps --skip-build` | PASS / PASS / PASS: 247 total, 245 passed, 0 failed, 2 expected environment-dependent skips | PASS / PASS | PASS: generated dummy fixture, bundled library, catalogs, resources, help, and licenses | PASS | PASS: ignored inherited preset listed and configured, then removed | **PASS** |
| WSL x64 | WSL unavailable; native-Linux build not reused | `c7fb7121434e3c499ee250c7b5afa732e8365040` target, not executed | Not run against target `c7fb7121434e3c499ee250c7b5afa732e8365040` | Not run | Not run | Not run | Not run | Names inspected only | Not run | **PENDING EXTERNAL LOCAL VALIDATION** |

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

The secret-store correction was externally rerun from a native Windows x64
clean checkout at GitHub-reachable head
`ee201dc5758df89c0a371e2060c339359181586d`, using the external classic vcpkg
root `C:\vcpkg`. The generated Debug and Release wxWidgets setup headers both
reported `wxUSE_SECRETSTORE=1`, and the secure-store CMake probe passed.
`setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild` passed, the
canonical `win-x64-debug-ninja` configure passed, and
`win-debug-build-ninja` completed all 2476 Ninja steps.

The complete Windows CTest run executed 249 tests: 213 passed and 36 failed.
Thirty-four failures were policy/shell tests whose required `rg` executable was
not on `PATH`. Two further failures came from test-only symlink assumptions in
`MacosSdkCacheGuard` and `MissingRipgrepBehavior`; a standard non-elevated
Windows process reported `WinError 1314`. These are test workflow and test
portability findings, not application build failures.

The focused correction makes ripgrep an explicit Debug/full-CTest preflight
requirement while leaving Release-only application builds unaffected and never
installing software. `MissingRipgrepBehavior` now creates a portable temporary `dirname` wrapper in
its isolated `PATH` instead of creating a symlink. The
macOS SDK cache-guard test still runs its alias assertions wherever symlink
creation is supported; when the operating system explicitly denies that
capability, it reports the reason and continues all non-alias assertions.

Windows remains **PENDING EXTERNAL LOCAL VALIDATION**. The corrected published
branch must be rerun externally through the full Debug CTest suite, Release
configure/build/stage, resource checks, local override, and final cleanliness
check before this row can become PASS.

## Pending external execution

Windows still requires clean native execution of the classic-vcpkg launcher,
MSVC/Git Bash checks, Debug build and CTest, Release build/stage, resources, and
local override. Apple Silicon macOS still requires both builds, Debug CTest,
gettext/resources, external vcpkg, and a local override. WSL must independently
exercise the launcher, Debug/CTest, Release/stage/resources, and `/mnt/c`
isolation from a clean WSL-filesystem checkout. CI cannot promote these rows.

Normal PR CI remains required before merge, but it was not used as local
evidence and was not available when this record was written. ORG-040 release
and installer validation and ORG-041 checklist finalization were not started.
