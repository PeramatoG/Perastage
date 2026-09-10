# Main branch protection contract

CH-003 is operationally complete. The `Protect main` repository ruleset is active,
the dedicated release App has been validated and used by a real patch-version run,
and classic branch protection is not configured. This document records the live
contract and its security/rollback rationale; it is not an activation checklist.

## Active ruleset

The live repository ruleset is:

| Setting | Active value |
|---|---|
| Name / ID | `Protect main` / `20119238` |
| Enforcement / target | Active on `~DEFAULT_BRANCH`, with no exclusions |
| Deletion / non-fast-forward | Both blocked |
| Pull request | Required, with `0` approvals and conversation resolution required |
| Merge methods | Merge, squash, and rebase allowed |
| Required checks | `linux-debug`, `macos-debug`, and `windows-debug`, each from GitHub Actions Integration ID `15368` |
| Strict status checks | Disabled (`strict_required_status_checks_policy = false`) |
| Admin bypass | Repository role actor ID `5`, `pull_request` mode |
| Automation bypass | Perastage Release Automation, Integration actor ID `4902198`, `always` mode |

Classic branch protection is not configured. The ruleset above is therefore the
single branch-protection policy affecting `main`, rather than one layer of two
potentially conflicting mechanisms. Coverage and other informational workflows are
not required checks.

The three required contexts are the platform job IDs in `CI Debug Tests`. They test
the exact SHA produced by `resolve-source`; requiring `resolve-source` separately
would be redundant. Loose checks avoid repeating three expensive platform builds
after every unrelated main update. A maintainer should still refresh a branch when
a concurrent change touches the same build or behavior.

## Dedicated release identity

The sole Integration with `always` bypass is the dedicated GitHub App:

| Property | Verified value |
|---|---|
| Name | Perastage Release Automation |
| Slug | `perastage-release-automation` |
| Integration actor ID | `4902198` |
| Installation ID observed during validation | `160710316` |
| Installation scope | `PeramatoG/Perastage` only |
| Requested repository permission | `contents: write` |

The manual validation workflow confirmed this scope without performing a Git write.
A subsequent real `Main Patch Release Artifacts` `bump-version` run authenticated as
the App and advanced `VERSION` to `1.6.32`.

Only the patch `bump-version` and minor-release `publish-release` jobs can mint the
App token. They request only repository contents write access and fail closed when
App mode is enabled but token creation fails. Builders, temporary-reference staging,
cleanup, and recovery do not receive this credential. Commit attribution does not
control authorization; the checkout credential authenticating the push does.

The built-in GitHub Actions App must not receive `always` bypass. Such a bypass would
apply actor-wide and would let any present or future workflow with a write-capable
`GITHUB_TOKEN` bypass the pull-request and check rules. The dedicated, repository-only
App confines that authority to credentials available to the two intended publishers.
The admin-role bypass remains pull-request-only and cannot authorize direct main
writes.

## Git-state writers

Three workflows request `contents: write`:

- `Main Patch Release Artifacts` may advance `main` by one validated patch increment.
  Its `bump-version` job uses the dedicated App in active App mode.
- `Minor Draft Release` stages and validates all release outputs, then its final
  publisher uses the dedicated App for the atomic main/tag push and draft Release.
- `Recover Validated Minor Release` may recover a validated tag and draft Release
  reachable from current `main`; it cannot update `main` and has no main bypass.

`CI Debug Tests` has read-only repository access. It runs on pull requests, pushes,
manual dispatch, and reusable-workflow calls, but performs no repository, tag, or
Release write.

## Rollback and incident response

If release-App authentication or publication behaves unexpectedly:

1. Disable `PERASTAGE_RELEASE_APP_ENABLED` to stop selecting the App credential.
2. Disable or suspend the App installation if its key or installation is suspect.
3. Remove Integration actor `4902198` from the ruleset before investigating any
   possibility of unintended branch access.
4. Keep pull-request, required-check, deletion, and non-fast-forward rules active.
5. Rotate the App private key before re-enabling it after a credential incident.
6. Re-run the no-write validation workflow and verify repository-only scope before a
   controlled publisher validation.

Removing the App bypass intentionally prevents the automated direct main writers; it
does not require weakening ordinary branch protection. If emergency repository work
is unavoidable, use the existing administrator pull-request path and retain an audit
trail rather than granting the built-in Actions App broad bypass.

Do not add signed-commit, linear-history, deployment, code-scanning, code-owner,
merge-queue, branch-name, metadata, path, or file-size rules without a separate design
and validation. Tags may receive a separate future policy; recovery does not justify a
main-branch bypass.
