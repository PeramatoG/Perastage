# Maintainer Runbook

This runbook is the operational index for maintaining Perastage. It routes each
normal operation and failure mode to its current canonical owner; it does not
replace those detailed procedures. Follow the linked documents for commands and
policy. Repository workflows and tests are authoritative for machine behavior,
and volatile workflow internals must not be copied into this index.

## Routine operations

| Operation | Purpose or trigger | Canonical owner | Caution |
| --- | --- | --- | --- |
| Prepare a local development environment | Set up a supported toolchain and dependencies. | [Build and Dependency Guide](build.md) | Use the supported platform setup or preset path. |
| Build Perastage locally | Configure and compile a development checkout. | [Build and Dependency Guide](build.md) | Keep machine-specific overrides untracked. |
| Run tests or focused validation | Check a touched behavior or reproduce a failure. | [Build and Dependency Guide](build.md) and the relevant repository test | Run focused local checks first; hosted CI is the clean-environment authority. |
| Validate a pull request | Review the change and its cross-platform Debug results. | [Pull requests](github_actions_workflows.md#pull-requests) and `CI Debug Tests` | Required jobs test the resolved pull-request SHA. |
| Diagnose hosted CI failures | Identify a platform, build, test, policy, or infrastructure failure. | [GitHub Actions workflow architecture](github_actions_workflows.md) | Inspect the current job summary, artifacts, and logs before changing code. |
| Build or package locally | Reproduce a platform package or integration issue. | [Packaging](packaging.md), then [Build](build.md) for prerequisites | Do not infer signing or notarization beyond the documented current state. |
| Inspect automatic patch artifacts after merge | Test packages produced from an accepted `main` update. | [Merges into main](github_actions_workflows.md#merges-into-main) and `Main Patch Release Artifacts` | These retained workflow artifacts are not permanent release assets. |
| Build compatibility packages | Produce the maintained secondary-platform packages. | [Weekly and manual compatibility packages](github_actions_workflows.md#weekly-and-manual-compatibility-packages) and `Weekly Compatibility Packages` | Resolve and record the intended source ref. |
| Prepare a minor draft release | Validate all release packages before publishing Git state and a draft Release. | [Minor draft releases](github_actions_workflows.md#minor-draft-releases) and `Minor Draft Release` | Use the documented dry-run and transactional path. |
| Recover a validated minor release whose publication failed | Publish already validated assets without rebuilding or calculating a new version. | [Recovering a validated but unpublished minor release](github_actions_workflows.md#recovering-a-validated-but-unpublished-minor-release) and `Recover Validated Minor Release` | This is different from a normal minor release; do not force-push or reconstruct it manually. |
| Diagnose release authentication or protected-`main` publication | Resolve failures involving the dedicated publication identity or ruleset. | [Main branch protection](main_branch_protection.md) | Follow its rollback and incident-response contract. |
| Update documentation after behavior changes | Keep user, developer, and release documentation synchronized. | [Documentation Policy](documentation_policy.md) | Update the canonical owner rather than creating parallel guidance. |

Dependency or cache outages do not by themselves justify changing a dependency
baseline, vendoring policy, or cache architecture. First establish whether the
affected remote is required, a performance optimization with fallback, or an
optional runtime integration.

## When something fails

| Failure | Route |
| --- | --- |
| Local configure or build failure | Use the platform prerequisites and troubleshooting in [Build](build.md). |
| Hosted CI or test failure | Use [GitHub Actions workflow architecture](github_actions_workflows.md), then inspect that run's current job summary, artifacts, and logs. |
| Packaging failure | Use [Packaging](packaging.md) plus the relevant builder or orchestration section in [GitHub Actions workflow architecture](github_actions_workflows.md). |
| Minor-release builders and assets validated, but publication failed | Dispatch `Recover Validated Minor Release` only under its documented preconditions in [the recovery procedure](github_actions_workflows.md#recovering-a-validated-but-unpublished-minor-release). |
| Dedicated release GitHub App or protected-`main` publication failure | Use the rollback and incident-response contract in [Main branch protection](main_branch_protection.md); do not bypass or broaden credentials ad hoc. |
| External service outage | Classify the service below as blocking, fallback-capable, or optional before changing repository policy or code. |

## External service dependencies

This inventory records operational purpose, not credentials. Failure effects
describe the affected capability; a service can be blocking for that capability
without blocking unrelated local development.

| Service or platform | Purpose | Used by | Failure effect | Current owner or reference |
| --- | --- | --- | --- | --- |
| GitHub repository and GitHub Actions | Source hosting, pull requests, CI, workflow artifacts, issue maintenance, and release orchestration. | Maintainers, contributors, and repository workflows | Blocks the corresponding hosted operation; an existing checkout can still support local work. | [GitHub Actions workflow architecture](github_actions_workflows.md) |
| GitHub Releases | Publication and download of release assets. | Minor release publication and users downloading releases | Blocks release publication or asset access, not local build and test. | [Minor draft release publication and recovery](github_actions_workflows.md#minor-draft-release-publication-and-recovery) |
| GitHub Packages repository NuGet feed | Persistent vcpkg binary-cache layer for CI and package workflows. | CI/package dependency installation and the trusted cache publisher | Reads accelerate supported jobs and can fall back to local builds when configured; only trusted cache jobs publish. An outage can increase build time without changing dependency policy. | [Build](build.md), current workflows, and `vcpkg-binary-cache.yml` |
| `Perastage Release Automation` GitHub App | Narrowly scoped identity for authorized protected-`main` and release publication. | The designated patch and minor-release publisher jobs | Authentication failure blocks those Git-state writes and fails closed; builders and ordinary local work are unaffected. | [Main branch protection](main_branch_protection.md) |
| GitHub Pages and the custom documentation domain | Host the public documentation at `perastage.luismaperamato.com`. | Documentation readers and repository Pages delivery | A Pages or domain outage makes public docs unavailable but does not block build, test, package, or release workflows. | [`docs/CNAME`](../CNAME) and repository Pages settings |
| GDTF Share | Optional online fixture catalog, login, and GDTF download integration. | Perastage runtime network client | Its failure disables that online workflow; local projects, local GDTF files, builds, and releases remain available. | `core/gdtfnet.cpp` and [GDTF Share catalog ingestion](gdtf_share_catalog.md) |
| Native OS secure credential stores | Secure local persistence of GDTF Share credentials where available. | Runtime credential-store integration and release-gate verification | Store unavailability prevents or limits secure persistence; platform checks and runtime provider requirements determine supported behavior. | [Secure credential-store verification](build.md#secure-credential-store-verification) and `core/credentialstore.cpp` |
| Upstream dependency and package download services | Acquire toolchain-managed source archives and packages. | vcpkg and platform build/bootstrap tooling | A cache miss or upstream outage can block fresh dependency acquisition; existing local caches may permit work. | [Build](build.md) and current package/workflow definitions |

### Authentication and secrets

Authentication is used only for the capabilities described above. Keep secret
values, tokens, passwords, private keys, and credential contents out of tracked
files and documentation. Do not copy credentials for continuity. Repository
configuration and workflow definitions own public configuration names; the
[main branch protection contract](main_branch_protection.md) owns release-App
scope, credential placement, rollback, and incident response. GDTF Share user
credentials belong only in the supported native secure-store flow.

## Single-maintainer operating model

[CODEOWNERS](../../.github/CODEOWNERS) intentionally records one current
repository maintainer/owner. Do not invent a second reviewer or owner, or encode
fake redundancy in CODEOWNERS, CI, or release workflows. The lack of a second
maintainer is an operational continuity risk, not a software-correctness or
code-health defect.

Repository-visible procedures and machine-enforced workflows reduce dependence
on individual knowledge. They do not justify committing account-recovery data,
private credentials, or other sensitive material. If contributors later assume
stable responsibility for defined areas, update CODEOWNERS and maintainer
documentation to reflect that real ownership.

## Canonical ownership boundary

Keep this runbook concise by linking rather than repeating:

- exact configure, build, and test commands belong in [Build](build.md);
- package and platform details belong in [Packaging](packaging.md);
- CI architecture, artifacts, releases, and recovery mechanics belong in
  [GitHub Actions workflow architecture](github_actions_workflows.md);
- protected-`main`, release identity, and publication security belong in
  [Main branch protection](main_branch_protection.md);
- module and path ownership belong in [Architecture](architecture.md) and
  [Repository Layout](repository_layout.md); and
- documentation synchronization belongs in
  [Documentation Policy](documentation_policy.md).

When machine behavior and prose differ, verify the current repository workflow
or test, then correct the canonical owning document rather than expanding this
index with a second procedure.
