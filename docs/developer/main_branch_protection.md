# Main branch protection contract

This document records the CH-003A audit of GitHub protection and proposes the
configuration for a later CH-003B. It complements, rather than repeats, the
[GitHub Actions workflow architecture](github_actions_workflows.md).

> **Audit-only status:** CH-003A did not create, edit, disable, or delete a
> ruleset or classic branch-protection rule. It did not change any release or
> versioning workflow. CH-003B is **not ready to apply** until an administrator
> verifies classic protection and provisions or identifies a narrowly scoped
> automation identity.

## Audit basis and access limits

The audit was performed on 2026-09-10 at `origin/main` commit
`93adfc05b63e420ae37672295eaf5efc5a760ab3`, after CH-002 was merged as PR
#2344. Current authenticated ruleset detail, the public GitHub REST API, and the
checked-out workflows were inspected. This environment had no authenticated
GitHub CLI session and could neither administer nor modify repository settings.
Classic branch protection was not accessible with the current integration, so an
administrator must determine its state in the Settings UI before CH-003B.

## Current protection of `main`

The public ruleset list and the effective-rules endpoint showed one active,
repository-level branch ruleset affecting `main`:

| Setting | Current value |
|---|---|
| Name / ID | `Protect main` / `20119238` |
| Enforcement | Active |
| Target condition | Include `~DEFAULT_BRANCH`; exclude nothing |
| Deletion | Blocked |
| Non-fast-forward updates | Blocked |
| Pull request required | No |
| Required status checks | None |
| Bypass actors | Repository admin role (`RepositoryRole`, actor ID `5`), `pull_request` mode; `current_user_can_bypass` was `pull_requests_only` |

The current admin-role bypass applies only in the pull-request context. It is not
an unrestricted direct-push bypass to `main` and therefore cannot authorize the
direct main writes required by `Main Patch Release Artifacts` and `Minor Draft
Release`. It is nevertheless part of the active ruleset and must be retained or
deliberately reconsidered in CH-003B.

No other repository or organization ruleset appeared in the effective rules for
`main`; the only effective rules returned were deletion and non-fast-forward.
This is strong evidence that no overlapping ruleset currently applies. It does
not prove that classic branch protection is absent because that setting was not
accessible with the current integration. Before applying CH-003B, an
administrator must inspect **Settings > Rules > Rulesets** and **Settings >
Branches**, record any classic rule, and reconcile it rather than creating an
accidentally layered policy.

## Git-state writer inventory

Only three current workflows request `contents: write`. Their Git/GitHub writes
are inline workflow commands; the asset assembly and validation scripts invoked
by them only prepare or validate runner-local files.

| Workflow | Trigger and permissions | Remote writes and identity | Preconditions and architectural need |
|---|---|---|---|
| `Main Patch Release Artifacts` (`main-patch-test-build.yml`) | Push to `main` or manual dispatch; `contents: write`, `actions: read`, `packages: read`; serialized by `main-patch-version-bump` with cancellation disabled | The checkout-provided `GITHUB_TOKEN` pushes `HEAD:main` as the GitHub Actions installation (`github-actions[bot]`). It writes no tag, temporary ref, or Release. | Validates numeric `VERSION`, makes one patch increment, marks the commit to avoid recursion, and excludes bot-triggered push runs. The direct main write is required by the current automatic post-merge version/artifact architecture. |
| `Minor Draft Release` (`minor-draft-release.yml`) | Manual dispatch; `contents: write`, `actions: read`, `packages: read`; serialized by `perastage-minor-draft-release` | The checkout-provided `GITHUB_TOKEN` creates and deletes exactly `refs/heads/automation/release-v<version>-<run-id>`, then may atomically fast-forward `refs/heads/main` and create `refs/tags/v<version>`. `github.token` is also passed to `gh` to create/edit a draft GitHub Release and upload assets. | Validates version/tag state and the artifact contract, builds all packages from the exact staged SHA, validates the assembled assets and provenance, refetches and requires `origin/main == BASE_SHA`, and normally publishes main plus tag with `git push --atomic`. Retry states are explicitly constrained. Every write is part of the stabilized minor-publication transaction or exact temp-ref cleanup. |
| `Recover Validated Minor Release` (`recover-minor-release.yml`) | Manual dispatch; `contents: write`, `actions: read`; serialized by `perastage-minor-release-recovery` | The checkout-provided `GITHUB_TOKEN` may create only the requested `refs/tags/v<version>` and may create/edit a draft GitHub Release and upload assets. It never updates `main` or a temporary branch. | Requires a full SHA and semantic version, requires that SHA to be reachable from current `origin/main`, verifies `VERSION`, the completed source `Minor Draft Release` run, the single unexpired validated artifact, checksums, and provenance. Dry run defaults to true. These restricted writes recover publication without rebuilding or moving main. |

`CI Debug Tests` (`ci-tests.yml`) runs on pushes and pull requests to `main`, on
manual dispatch, and as a reusable workflow. It has only `contents: read` and
`packages: read`; it fetches refs and tests an exact resolved SHA but performs no
repository, tag, or Release write. No other workflow currently requests
`contents: write` or contains a Git/GitHub remote-publication command.

The visible commit author configuration is not the authorization boundary.
`git config user.name github-actions[bot]` controls commit/tag attribution, while
the credential persisted by `actions/checkout` is the repository's
`GITHUB_TOKEN`: an installation access token for the GitHub Actions App. Ruleset
bypass must therefore address the App/integration that authenticates the push,
not the configured Git author.

## Pull-request check contract

The head SHAs of five consecutive successful PRs targeting `main` (#2340–#2344)
all reported the same four check-run names and producer:

| Check name | Producer | Gate classification |
|---|---|---|
| `linux-debug` | GitHub Actions, App ID `15368` | Required: complete Linux Debug validation |
| `macos-debug` | GitHub Actions, App ID `15368` | Required: complete macOS Debug validation |
| `windows-debug` | GitHub Actions, App ID `15368` | Required: complete Windows Debug validation |
| `resolve-source` | GitHub Actions, App ID `15368` | Prerequisite only: resolves the exact SHA and cache trust; the three platform jobs already depend on it |

The smallest complete gate is therefore the three platform checks. Their job IDs
are literal workflow keys, were stable across the sampled PRs, and each validates
the exact head SHA emitted by `resolve-source`. Requiring `resolve-source` too
would be redundant and would add another name that can block merging without
adding platform coverage. CH-003B should bind each required context to the
expected GitHub Actions App (ID `15368`) if the ruleset UI/API offers expected
source selection.

Use **loose** status checks: do not require the PR branch to be up to date before
merging. The checks validate the exact proposed head SHA, while strict mode would
re-run relatively expensive Windows and macOS jobs after every intervening main
change. For this single-maintainer repository, that cost is disproportionate.
Loose mode accepts the residual risk that a passing head can interact badly with
a newer `main`; the maintainer should update and rerun when a concurrent change
touches the same behavior or build surface.

## Proposed CH-003B ruleset

Subject to the unresolved bypass and classic-rule prerequisites below, edit the
existing ruleset rather than layer a second one:

| Setting | Proposed value and reason |
|---|---|
| Name | Keep `Protect main` |
| Target / enforcement | Branches matching `~DEFAULT_BRANCH`, no exclusions; Active |
| Require a pull request before merging | Enabled, so ordinary human changes use the reviewed PR path |
| Required approvals | `0`; there is one primary maintainer, so an invented second reviewer would impede rather than improve the real process |
| Dismiss stale approvals / code-owner approval / last-push approval | Disabled; none adds value with zero required approvals |
| Require conversation resolution | Enabled; unresolved review findings should not be merged, without requiring another person to approve |
| Required status checks | `linux-debug`, `macos-debug`, `windows-debug`, each expected from GitHub Actions App ID `15368` |
| Require branches to be up to date | Disabled (loose), for the cost/risk balance above |
| Block deletions | Enabled (retain current rule) |
| Block force pushes / non-fast-forward updates | Enabled (retain current rule) |
| Existing admin bypass | Retain repository admin role (`RepositoryRole`, actor ID `5`) in Pull requests only mode; it does not authorize direct pushes |
| Automation bypass | A **dedicated release-automation GitHub App**, Always allow, used only by the main-writing steps in `Main Patch Release Artifacts` and `Minor Draft Release` |

Do not enable signed commits until the automation signing design is established;
do not enable linear history because normal merge commits are current practice;
do not add deployment, code-scanning, code-owner, merge-queue, branch-name,
metadata, file-path, file-size, or restricted-push rules because they are not
needed for this contract. Do not give recovery a main-ruleset bypass: its tag and
Release writes do not target the protected branch. Tags can receive a separate
future policy, but that is outside CH-003B.

### Bypass decision and unresolved prerequisite

Ruleset bypass modes are actor-wide, not workflow-wide. **Pull requests only** is
insufficient because both trusted publishers directly update `main`. Adding the
built-in GitHub Actions App with **Always allow** would let any present or future
workflow holding a write-capable `GITHUB_TOKEN` bypass the PR and status-check
rules on `main`; GitHub cannot narrow that actor entry to two workflow files.
That would fail scenario J and is an explicit, broad-authority security tradeoff,
not the safe proposed configuration.

The narrower design is a dedicated GitHub App installed only on this repository,
granted only the minimum repository-contents write permission, selected in the
ruleset as an Integration bypass actor with **Always allow**, and minted only in
the two intended publication workflows from protected App credentials. Ordinary
workflows retain the built-in `GITHUB_TOKEN` and therefore cannot use the bypass.
App private-key access and workflow modification remain sensitive administrative
boundaries. An administrator must confirm that this dedicated installed App is
selectable in this repository's bypass UI before CH-003B; it does not currently
exist in the audited tree or visible public settings. Until that confirmation,
the safe exact bypass identity remains unresolved and CH-003B is not ready.
`Always allow` necessarily lets that dedicated identity bypass every rule in this
main-targeting ruleset, including deletion and non-fast-forward rules; narrowing
which workflows can mint its token and preserving the workflows' explicit
fast-forward-only commands are therefore essential parts of the trust boundary.

If a dedicated App is not acceptable, the least-disruptive alternative is to
restructure the patch bump as an automatically opened PR and merge it only after
the required checks. The minor release could similarly stage a protected PR and
publish the tag/Release after merge, but that changes the current atomic
main-and-tag guarantee and must not be adopted casually. A personal access token
or broad built-in Actions bypass is not a narrower substitute. No normal process
should require temporarily disabling protection.

## Scenario matrix

These results describe the proposed dedicated-App design, not the unchanged
current ruleset.

| Scenario | Expected result |
|---|---|
| A. Normal PR; all three platform Debug checks pass | Allowed once conversations are resolved; no approval is mandatory. |
| B. Normal PR; `windows-debug` fails | Blocked by the required check. |
| C. Normal PR; a required check is missing | Blocked until the exact expected check reports success. |
| D. Direct human push to `main` | Blocked by the PR requirement. |
| E. Force push to `main` | Blocked for humans and non-bypass automation. The dedicated App is technically exempt under Always allow, so its token confinement and unchanged fast-forward-only commands are required. |
| F. Delete `main` | Blocked for non-bypass actors. The release App could technically bypass this rule but no intended workflow issues a main deletion. |
| G. Automatic post-merge `VERSION` bump | Allowed only when its main push uses the dedicated App; serialization, validation, and recursion suppression remain unchanged. |
| H. Minor Draft Release | Its temp ref remains outside the target; after package/asset validation and the `origin/main == BASE_SHA` check, the dedicated App can retain the atomic fast-forward-main/tag push. |
| I. Recovery workflow | No main bypass is needed or granted; it remains limited to a release commit already reachable from `main`, a tag, and a draft Release. |
| J. Unrelated workflow using `GITHUB_TOKEN` | Cannot bypass because the built-in GitHub Actions App is not a bypass actor. This guarantee is lost if CH-003B instead grants that App Always allow. |
