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

Perastage maintains a curated working draft for the next release in [`release-notes-draft.md`](../release-notes-draft.md).
Update that draft whenever a merged PR includes a meaningful user-facing change, such as a feature, bug fix, performance improvement, stability improvement, packaging change, or documentation update.
Internal-only changes can be omitted or recorded under the internal section when they may help maintainers review the release.

Before publishing a GitHub Release, review the draft, remove entries that are too technical or temporary, group related items, and convert it into concise public release notes.
Include the latest documentation link (`https://perastage.luismaperamato.com/`) in each published release note.

PR titles, labels, and the PR template release-note field should be clear because they help keep the curated draft accurate.

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

## Change History Policy

Perastage intentionally does not maintain a standalone manual `CHANGELOG.md`.
GitHub Releases are the canonical curated public history of published versions.
The release-notes draft is only the working source for the next release: maintainers
review it, remove excessive or internal detail, and the MINOR release workflow uses
the curated result as the draft GitHub Release body. The final GitHub Release, not
[`release-notes-draft.md`](../release-notes-draft.md), is the permanent public record.

Git commits, pull requests, tags, and compare views are the detailed engineering
history. Maintainers and coding agents should use those sources when they need
implementation details, rationale, changed files, tests, or a complete release
delta rather than reconstructing every merged pull request in a second manually
maintained chronological file. This avoids duplicated history and drift.

If a concrete need arises for a complete offline or repository-local release
history, prefer a generated artifact derived from tags, GitHub Releases, or pull
request metadata. Reconsider a standalone changelog only when a real consumer or
workflow need cannot be met by GitHub Releases and repository history.

## Main Branch PATCH Automation

PATCH is automatically incremented after normal accepted non-bot updates to `main` by the `Main Patch Release Artifacts` workflow.

The workflow validates `VERSION` using `MAJOR.MINOR.PATCH`, increments only PATCH, and commits the result back to `main` using `[skip version-bump]` to prevent a bump loop. It passes the generated commit's exact SHA to the three primary Release package builders:

- Windows Release installer.
- Ubuntu Release AppImage.
- Current macOS Release DMG.

These outputs are GitHub Actions test and verification artifacts, not official GitHub Release assets. This automation does not create an official Git tag or GitHub Release.

## Manual MINOR Draft Release Workflow

Perastage includes a manual workflow named `Minor Draft Release` in `.github/workflows/minor-draft-release.yml`.

This workflow is triggered only by `workflow_dispatch` and supports a dry-run mode.

It performs these actions for a MINOR release:

- Resolves current `main` to an exact base SHA, validates `VERSION`, and computes the next `MAJOR.MINOR.0` version and tag.
- For a real run, creates a run-specific temporary automation ref with a staged release commit containing the `VERSION` change.
- Builds all five maintained release packages from that exact staged release SHA: the Windows installer, Ubuntu AppImage, macOS 15 Apple Silicon DMG, current macOS Apple Silicon DMG, and Arch Linux x86-64 package.
- Treats package and final-asset validation as blocking requirements.
- Only after validation succeeds, verifies the expected `main` state and publishes the validated release Git state. Normal publication advances `main` to the staged commit and creates the annotated tag transactionally.
- Creates or updates a draft GitHub Release with the validated public assets.
- Uses `docs/release-notes-draft.md` as the release body when the file is present and non-empty.
- May use GitHub-generated notes when the curated draft is missing or empty.
- Cleans up its temporary release ref.

The workflow intentionally leaves the GitHub Release as a draft so the maintainer can manually review, edit, and publish it. It does not create MAJOR releases automatically.

See [GitHub Actions workflow architecture](github_actions_workflows.md) for the canonical workflow mechanics, transactional publication, artifact validation, and recovery procedures.
