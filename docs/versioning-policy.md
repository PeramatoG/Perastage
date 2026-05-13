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

## Recommended Future Automation

A future workflow may increment PATCH automatically after successful merges to `main` and upload test installers as workflow artifacts.

A future manual workflow may increment MINOR, reset PATCH to `0`, build installers for Windows/Linux/macOS, create a tag, create a GitHub Release, generate release notes, and upload installers.

A future manual MAJOR workflow may increment MAJOR, reset MINOR and PATCH to `0`, and create a draft release for manual review.
