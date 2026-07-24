# GitHub Actions workflow architecture

Perastage uses separate workflows for validation, automatic test artifacts, compatibility packages, and formal releases. The separation keeps Debug test coverage out of Release package builders and prevents release publication before every package and asset check has succeeded.

## Pull requests

Pull requests targeting `main` run `CI Debug Tests` from `.github/workflows/ci-tests.yml`.

- The workflow resolves the requested ref to one exact commit SHA and every Debug job checks out that SHA.
- Linux Debug builds the complete test target set, runs the complete CTest suite, and runs repository policy tests.
- Windows Debug initializes Visual Studio Hostx64/x64 explicitly, uses Ninja with MSVC and the `x64-windows` vcpkg triplet, and rejects MinGW, MSYS, or Strawberry toolchain selection.
- Current macOS Debug uses the explicit current macOS runner, Ninja, arm64, and release-gate secure-store tests.
- Debug jobs configure with `BUILD_TESTING=ON` and do not define `NDEBUG`, because several tests rely on `assert()`.
- The workflow uploads diagnostics only on failure. Diagnostics include command logs, CMake configure logs, `CMakeCache.txt`, CTest logs, vcpkg failure logs, and a concise environment summary.

The Debug CI workflow does not upload installers, AppImages, DMGs, or platform packages.

## Merges into main

`Main Patch Release Artifacts` in `.github/workflows/main-patch-test-build.yml` runs after accepted non-bot pushes to `main` and can also be started manually.

The workflow serializes version mutation, increments only the patch component in `VERSION`, commits the bump with `[skip version-bump]`, and passes the exact generated commit SHA to the three primary Release builders:

- Windows Release installer.
- Ubuntu Release AppImage.
- Current macOS Release DMG.

It does not rerun Debug CI, and it does not build macOS 15 or Arch Linux compatibility packages. These artifacts are intended for manual testing and continuous verification of current hosted packaging runners. Automatic patch artifacts use GitHub Actions artifact retention and are not permanent release assets.

## Weekly and manual compatibility packages

`Weekly Compatibility Packages` in `.github/workflows/compatibility-builds.yml` runs weekly at `03:27 UTC` on Tuesday and supports `workflow_dispatch` with an optional `source_ref`.

The workflow resolves `source_ref` or `main` to one exact SHA, does not modify `VERSION`, and builds only the secondary compatibility packages:

- macOS 15 arm64 Release DMG.
- Arch Linux x64 Release package.

A newer compatibility run for the same ref cancels an older in-progress compatibility run. The individual macOS 15 and Arch builder workflows remain manually runnable when a maintainer needs to reproduce one package.

## Minor draft releases

`Minor Draft Release` in `.github/workflows/minor-draft-release.yml` is manual. It keeps the existing release title, prerelease, and dry-run inputs.

The release pipeline is staged:

1. Resolve current `main` to an exact base SHA, validate `VERSION`, compute the next `MAJOR.MINOR.0` version and tag, verify the tag does not already exist, and validate the artifact contract.
2. Create a temporary automation ref based on the resolved base SHA and commit only the `VERSION` update there. Debug CI is diagnostic while the historical suite is being repaired, so the minor release package builders do not wait for or depend on Debug test success.
3. Build all five Release packages from the staged release commit SHA: Windows, Ubuntu AppImage, macOS 15, current macOS, and Arch Linux. Package builder failures remain blocking.
4. Download and validate all package and symbol artifacts, reject duplicates or stale filenames, assemble the unified developer-only debug-symbol archive from the actual platform symbol files, and create the final validated release-assets artifact. Asset validation failures remain blocking.
5. Publish only after validation succeeds by verifying `origin/main` still equals the base SHA, fast-forwarding `main` to the staged release commit, creating the annotated tag at that commit, and creating a draft GitHub Release with the validated assets.

The release remains transactional: no tag, `main` update, or draft GitHub Release is created until every required package and asset validation succeeds. If `main` changes while packages are building, publication fails clearly before the tag or draft release is created. The maintainer should rerun the workflow from the updated `main`. Temporary automation refs are deleted after success and by the cleanup job after failure.

Dry runs are inexpensive: they compute and report the version, tag, title, and artifact contract status without pushing a branch, committing, tagging, building packages, or creating a release.

## Stable artifact contract

The package filename patterns are centralized in `.github/release-artifact-contract.json` and validated by `tests/check_ci_workflow_architecture.py`.

Stable package patterns:

- `Perastage_*_Setup.exe`
- `Perastage-*-x86_64.AppImage`
- `Perastage-*-macOS15-arm64.dmg`
- `Perastage-*-macOS26-arm64.dmg`
- `Perastage-*-arch-x86_64.pkg.tar.zst`

Stable artifact names:

- `Perastage-windows-installer`
- `Perastage-linux-appimage`
- `Perastage-macos15-dmg`
- `Perastage-macos26-dmg`
- `Perastage-arch-package`
- `Perastage-windows-symbols`
- `Perastage-linux-symbols`
- `Perastage-macos15-symbols`
- `Perastage-macos26-symbols`
- `Perastage-archlinux-symbols`

Builder workflows must preserve the package filenames and symbol artifact structures unless the producer and collector contract is updated and tested in the same change.
