# Code Health

This document is the living source of truth for Perastage code-health
principles. It defines the durable maintenance contract; exact repository
inventories and limits remain owned by their machine-enforced policies.
Documentation ownership and maintenance rules remain in
[Documentation Policy](documentation_policy.md).

## Refactoring principles

- Refactor by responsibility, not by line count alone. Prefer small, focused
  extractions at the code path being changed over broad rewrites.
- Protect risky refactors with characterization or regression tests before
  changing structure, especially around import, export, rendering, project
  mutation, and other compatibility-sensitive behavior.
- An oversized file that still has one coherent responsibility is not split
  merely to meet a numerical target. Its size is a review signal; extraction
  is appropriate when a separable responsibility is identified.
- New services and policy logic should be GUI-independent where practical.
  Core owns reusable application behavior, while GUI modules own presentation
  and workflow wiring. Existing dependency directions and known debt are
  documented in [Architecture](architecture.md).

## Automated guardrails

[`tests/source_file_size_policy.json`](../../tests/source_file_size_policy.json)
is authoritative for the current default source-size threshold and the exact
hotspot baselines. `tests/check_source_file_size.py` applies that policy as a
ratchet to tracked project C/C++ files: existing exceptions may stay within
their reviewed baselines, while reductions are not automatically written back
to the policy. Do not duplicate the volatile hotspot inventory in prose.

Repository artifact and binary hygiene is owned by
[`tests/repository_hygiene_policy.json`](../../tests/repository_hygiene_policy.json)
and enforced by
[`tests/check_repository_hygiene.py`](../../tests/check_repository_hygiene.py).
Intentional binary asset classes and exceptional file-size limits require
narrow, reviewed policy entries; generated outputs and caches remain untracked.
The check audits the tracked tree, not historical Git objects.

Pull requests should run focused checks for the affected boundaries plus the
repository guardrails required by `AGENTS.md`. Hosted CI remains authoritative
for clean-environment and repository-wide validation. See
[GitHub Actions workflow architecture](github_actions_workflows.md) for the
current CI contract and [Build](build.md) for local build and test entry points.

## Architecture and compatibility direction

Use [Architecture](architecture.md) for module ownership and dependency
direction, and [Repository Layout](repository_layout.md) for path and build
ownership. Continue extracting reusable Core/services away from GUI and
toolkit dependencies rather than introducing new presentation coupling.

MVR and GDTF work must keep standards-compliant behavior separate from
deliberate compatibility handling. Preserve standard data as authored, isolate
legacy recovery or vendor-specific behavior behind explicit compatibility
paths, and characterize existing behavior before risky parser, serializer, or
package refactors. The detailed contracts remain in the
[canonical MVR contract](technical-notes/canonical_mvr_contract.md),
[GDTF mutation policy](gdtf_mutation_policy.md), and related focused technical
notes; this page does not replace them.

## Historical evidence

The [February 2026 code-health review](code_health_review_2026-02-13.md) is
retained as historical evidence. Its measurements, hotspot list, and suggested
backlog describe that point in time and are not current policy.
