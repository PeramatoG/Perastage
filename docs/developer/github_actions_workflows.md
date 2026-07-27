# GitHub Actions workflow architecture

Perastage uses separate workflows for validation, automatic test artifacts, compatibility packages, and formal releases. The separation keeps Debug test coverage out of Release package builders and prevents release publication before every package and asset check has succeeded.

## Issue maintenance

`Awaiting Feedback Triage` runs daily and can be dispatched manually in a safe dry-run mode. It processes only open issues explicitly labeled `status: awaiting-feedback`; see the [issue triage policy](../../.github/ISSUE_TRIAGE.md) for its conservative warning and closure rules. The workflow has read-only repository access plus issue write access and does not process pull requests or apply the label itself.

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

## Minor draft release publication and recovery

The `Minor Draft Release` workflow publishes Git state only after all installer builders and final asset validation have succeeded. The publisher configures the repository-local Git identity as `github-actions[bot]` before creating annotated tags so fresh GitHub-hosted checkouts can run `git tag -a` without depending on machine-global configuration.

Normal publication is intentionally transactional on the first publish attempt. It fetches `main` and tags, verifies that `origin/main` still equals the resolved base SHA, creates the annotated release tag locally at the validated release SHA, and pushes `main` plus the tag with one `git push --atomic` command. There is no non-atomic fallback; if the server rejects the atomic update, the workflow fails without intentionally publishing only half of the Git state.

The normal publisher is retry-safe for the known safe states:

- `origin/main` is still the base SHA and the tag is absent: create the annotated tag and atomically push `main` and the tag.
- `origin/main` is already the release SHA and the tag peels to that same release SHA: treat Git publication as complete and continue to draft release creation.
- `origin/main` is already the release SHA and the tag is absent: treat this as the legacy partial state from the v1.5.0 publication failure, create the exact annotated tag, and push only the tag.

Any other `main` SHA, any tag that peels to a different commit, or any situation requiring a force push is rejected. Tag checks use the peeled commit target (`tag^{commit}`), not the tag object's SHA, because release tags are annotated.

Draft release creation is also idempotent. The publisher checks `gh release view` before creating a release. If no release exists, it creates a draft and attaches only the public final assets. If a matching draft exists, it updates the title, notes, and prerelease state and reuploads assets with `--clobber`. If a non-draft release exists, publication stops rather than editing an already-published release or creating a duplicate.

### Recovering a validated but unpublished minor release

Use `Recover Validated Minor Release` only when a `Minor Draft Release` run completed all builders and `validate-release-assets` but failed during publication. The recovery workflow is manual-only, never calculates a new version from current `main`, never updates `main`, and never rebuilds installers in the normal recovery path. It downloads only the `Perastage-validated-release-assets` artifact from the supplied source run and places it under `release-assets/final`.

For the v1.5.0 incident, rerunning the normal minor workflow after merge would read `VERSION` as `1.5.0` on `main` and calculate `1.6.0`, so recovery must be used instead. Run the recovery workflow with:

- `source_run_id=<SOURCE_RUN_ID>` from the failed workflow URL.
- `release_sha=c857665b99aacf9f466edd4416584dfb56ac1a1f`.
- `release_version=1.5.0`.
- `dry_run=true` first.

After the dry run validates the source run, commit identity, asset names, checksums, tag state, release state, and release notes source, run the same workflow inputs again with `dry_run=false`. Confirm afterwards that `v1.5.0` points to `c857665b99aacf9f466edd4416584dfb56ac1a1f`, the GitHub Release exists as a draft, all six final public assets are attached, and `main` was not moved by recovery.

### Validated artifact provenance

Future `Perastage-validated-release-assets` workflow artifacts include an internal `release-provenance.json` file. This file records the repository, workflow run ID and attempt, base SHA, release SHA, release version, tag, timestamp, final asset filenames, and SHA-256 checksum for each final asset. The provenance file is retained inside the workflow artifact to support safe future recovery, but it is excluded from public GitHub Release uploads unless the artifact contract is explicitly changed to publish it.

The v1.5.0 artifact predates provenance. The validator permits exactly one legacy missing-provenance exception for release SHA `c857665b99aacf9f466edd4416584dfb56ac1a1f` and release version `1.5.0`; other SHA/version pairs must include matching provenance.

## CI Debug sccache policy

`ci-tests.yml` is the sole sccache owner in PR 3A. Its Linux, Hostx64/x64 Windows, and ARM64 macOS Debug jobs install dependencies first and then use the pinned Mozilla action v0.0.10 commit `9e7fa8a12102821edf02ca5dbea1acd0f89a2696` to install sccache v0.15.0. Installer, packaging, release, and vcpkg Binary Cache workflows do not enable it. The native GHA backend uses `perastage-ci-debug-v1` plus stable runner, architecture, compiler/toolset, Debug, and macOS SDK boundaries; `SCCACHE_BASEDIRS` normalizes the absolute checkout root.

Pull requests use GitHub's native merge-ref cache scope, so their writes support PR reruns but cannot be consumed by `main` or sibling pull requests; compatible default-branch entries remain restorable by PR jobs. A trusted manual current-`main` dispatch uses the default-branch `gha-main` scope. Workflow calls, arbitrary manual sources, and ambiguous combinations disable GHA and use an unpersisted `disk-ephemeral` cache. The unsupported `SCCACHE_GHA_RW_MODE` variable is not used. Summaries and artifacts retain credential-safe statistics and warning-level error logs, never runtime tokens. Remote I/O is fail-open to compilation, while a missing launcher, wrong compiler, malformed JSON, or zero requests after a successful build fails the job. A generated bracket-quoted CMake initial cache transports the complete launcher path, including Windows `sccache.exe`; macOS also exports its compile database. An earlier configure/build failure remains authoritative while the always-run statistics step records diagnostics. Windows Debug alone selects CMP0141 `NEW` and embedded `Z7` object debug information for cache compatibility.

The latest 2026-07-25 PR run measured 1,814 Linux requests with 1,813 hits and one miss, plus 15 macOS requests with 15 hits. Native object caching is therefore active on both platforms. On Windows the official action's `SCCACHE_PATH` is extensionless even though the installed leaf is `sccache.exe`; the workflow resolves that exact executable before generating an initial cache containing sccache, exact MSVC and Git Bash paths, CMP0141, and embedded debug information. CMake configure, exact toolchain validation, and structural `Z7` validation now have independent credential-safe logs; the validator accepts CMake's observed `-Z7` and MSVC's equivalent `/Z7` spelling while rejecting exact `Zi` and `ZI` forms. The next PR run must prove Windows compilation and nonzero requests through that configuration. CTest commands and selections remain unchanged and uncached, including the complete Linux suite whose unrelated failures occur after successful compilation.

The exact cold/populate/warm validation and PR 3B decision gate are documented in [build.md](build.md#post-merge-two-run-validation). CTest results are never cached or skipped, so compare build-step duration separately and expect CTest duration to remain broadly unchanged.
