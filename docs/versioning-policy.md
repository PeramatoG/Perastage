# Perastage Versioning Policy

Perastage uses a three-number semantic version format:

`MAJOR.MINOR.PATCH`

The canonical version source is the repository root `VERSION` file.

The version number does not include a leading `v` in the `VERSION` file.

For official releases, Git tags should include a leading `v`, for example `v1.1.0`.

## Version Components

### MAJOR

Increment MAJOR for major public milestones.

Use MAJOR increments for large changes, substantial architectural changes, major UX changes, incompatible project format changes, or releases requiring special manual communication.

Example:

`1.7.42 -> 2.0.0`

### MINOR

Increment MINOR for official public releases.

Use MINOR increments when Perastage publishes a normal public release with installers and release notes.

When MINOR is incremented, PATCH resets to `0`.

Example:

`1.0.27 -> 1.1.0`

### PATCH

Increment PATCH for accumulated main-branch development builds, small fixes, improvements, and testable builds.

PATCH may advance on `main` without creating an official GitHub Release.

These builds may produce GitHub Actions artifacts for testing.

Example:

`1.0.0 -> 1.0.1 -> 1.0.2`

## Build Artifacts vs Official Releases

GitHub Actions artifacts generated from `main` are test builds.

They are not official releases.

They may be temporary and subject to GitHub artifact retention limits.

Official releases are created only when a release workflow creates a Git tag and a GitHub Release.

## Tag Policy

Do not create tags for every merge to `main`.

Tags are reserved for official public releases and should use this format:

`vMAJOR.MINOR.PATCH`

Examples:

- `v1.1.0`
- `v1.2.0`
- `v2.0.0`

## Release Notes

Release notes should be generated from merged PRs between the previous official release tag and the new release tag.

PR titles and labels should be clear because they feed release-note quality.

Suggested labels:

- `bug`
- `enhancement`
- `packaging`
- `installer`
- `windows`
- `macos`
- `linux`
- `mvr`
- `gdtf`
- `viewer2d`
- `viewer3d`
- `docs`
- `ci`
- `internal`

## Main Branch PATCH Automation

PATCH is automatically incremented after normal updates to `main` by the `Main Patch Version and Test Installer Builds` workflow.

The workflow validates `VERSION` using `MAJOR.MINOR.PATCH`, increments only PATCH, and commits the result back to `main`.

The automated commit message includes `[skip version-bump]`, and the workflow skips commits that include this marker to prevent bump loops.

The same workflow dispatches these existing installer workflows on `main` for test artifacts:

- `windows-installer.yml`
- `linux-installer.yml`
- `macos-installer.yml`

This automation does not create Git tags and does not create GitHub Releases.

## Manual MINOR Draft Release Workflow

Perastage includes a manual workflow named `Minor Draft Release` in `.github/workflows/minor-draft-release.yml`.

This workflow is triggered only by `workflow_dispatch`.

It performs these actions for a MINOR release:

- Validates the root `VERSION` format.
- Increments MINOR and resets PATCH to `0`.
- Commits the new `VERSION` to `main` with `[skip version-bump]` to prevent the automatic PATCH bump workflow from running on that commit.
- Creates and pushes an annotated release tag in the format `vMAJOR.MINOR.0`.
- Builds Windows, Linux, and macOS installers from the new release tag using the existing installer workflows.
- Creates a GitHub Draft Release for the new tag.
- Uses GitHub automatic release-note generation.
- Attaches the Windows installer, Linux AppImage, and macOS DMG assets to the draft release.

The workflow intentionally leaves the release as a draft so the maintainer can manually review, edit, and publish it.

This workflow does not publish the release automatically and does not create MAJOR releases.
