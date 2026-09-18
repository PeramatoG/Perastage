# Developer Documentation

This index identifies the current owner for each kind of maintainer-facing
information. Canonical documents define project-wide policy, while focused
contracts describe one subsystem without superseding their canonical parent.

## Canonical project-wide sources

| Area | Current owner |
| --- | --- |
| Architecture, module ownership, and dependency direction | [Architecture](architecture.md) |
| Repository paths and build ownership | [Repository Layout](repository_layout.md), with [`repository_structure_baseline.json`](repository_structure_baseline.json) as the machine-readable structural contract |
| Code-health and refactoring policy | [Code Health](code_health.md) |
| Build and dependency workflows | [Build and Dependency Guide](build.md) |
| Packaging and platform integration | [Packaging](packaging.md) |
| CI and release workflow architecture | [GitHub Actions workflow architecture](github_actions_workflows.md), with [Main branch protection](main_branch_protection.md) as the focused ruleset contract |
| Localization | [Localization](localization.md), with the [Localization glossary](localization_glossary.md) as its terminology companion |
| Documentation organization | [Documentation Policy](documentation_policy.md) |

The [repository tree](perastage_tree.md) is a navigation aid, not a second
architecture or repository-layout specification.

## Active subsystem and technical contracts

- **MVR:** the [Canonical MVR contract](technical-notes/canonical_mvr_contract.md)
  owns format and behavior rules. [MVR identity recovery](mvr_identity_recovery.md),
  [grouped transform mutation](grouped_transform_mutation.md),
  [attachment paths](truss_fixture_attachment_paths.md), and the
  [MVR technical-note entry points](technical-notes/index.md#mvr-contracts) cover
  narrower implementation responsibilities.
- **GDTF:** [GDTF Mutation Policy](gdtf_mutation_policy.md) and
  [GDTF resource identity](gdtf_resource_identity.md) own write and derivative
  identity rules. [GDTF Share catalog ingestion](gdtf_share_catalog.md) and the
  [GDTF technical-note entry points](technical-notes/index.md#gdtf-contracts)
  cover focused catalog, compatibility, and editor behavior.
- **Application and GUI:** [Startup Architecture](startup_architecture.md),
  [GUI Shortcut Architecture](gui_shortcut_architecture.md),
  [Storage Policy](storage_policy.md), and
  [Text-to-scene Rules](text_to_scene_rules.md).
- **Rendering and placement:** [Viewer coordinate and placement](viewer_coordinate_contract.md),
  [UI unit systems](ui_unit_systems.md), and the
  [viewer technical-note entry points](technical-notes/index.md#viewer-and-rendering-contracts).
- **Testing and other focused policies:** [Test fixture policy](test_fixture_policy.md),
  [PDF portability](pdf_portability.md), [fixture category](fixture_category_policy.md),
  and the remaining topic-specific documents in this directory. These apply
  only to their named subsystem and defer project-wide ownership to the table
  above.

The [issue triage policy](../../.github/ISSUE_TRIAGE.md) remains the focused
repository-maintenance contract for issue handling.
