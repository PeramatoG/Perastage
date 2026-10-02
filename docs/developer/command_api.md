# Command API Contract

## Ownership and scope

`core/command/` owns Perastage's GUI-independent semantic command contract.
`perastage_command_core` contains typed requests, structured results, execution
context abstractions, and the mutation transaction. The separate
`perastage_command_serialization` target owns JSON output and is the only one of
the two targets that uses the JSON dependency.

Human command text remains an adapter rather than the semantic API. The focused
`perastage_command_text_parser` target under `core/command/` owns the neutral,
GUI-independent Console syntax model and parser. It may consume Command Core's
structured diagnostic contract, while `perastage_command_core` never depends
on the text parser. Raw Console strings and parsed adapter syntax are therefore
not semantic `Request` objects.

The embedded Console consumes this neutral parser. The separate
`perastage_command_transform_text_adapter` target depends on both the parser
and semantic transform boundaries; the parser remains independently usable by
syntax-only consumers and does not depend on scene execution. The adapter
converts position values from the Console grammar's meters to semantic
millimeters, then invokes `perastage_command_transform`. The Console owns
project-context acquisition, structured-result presentation, and GUI refresh;
it does not own reusable transform or selection algorithms. The focused
`perastage_command_selection` target owns UUID-based selection updates and the
compatible selection-only `clear` operation. The
`perastage_command_selection_text_adapter` target resolves Console fixture IDs
and truss unit numbers against an explicitly supplied scene; lookup remains
outside the syntax-only parser.

## Semantic requests and identifiers

A request has a stable `commandId` and an ordered list of named, typed
arguments. Values are limited to booleans, signed 64-bit integers, doubles,
strings, and homogeneous lists of those numeric or string types. Requests do
not contain raw Console input or a recursive JSON value tree.

Machine-facing identifiers are stable technical English and are never
localized:

- command IDs use lowercase ASCII letter-led alphanumeric segments separated
  by dots, such as `scene.transform.position`;
- argument IDs use lowercase ASCII letter-led snake case, without leading,
  trailing, or consecutive underscores;
- diagnostic codes use dotted lowercase ASCII technical identifiers; each
  non-empty segment may use snake case without leading, trailing, or
  consecutive underscores.

Concrete identifiers are introduced with concrete commands; Command Core does
not maintain a speculative registry.

## Scene selection commands

`scene.selection.update` uses an explicit object kind and UUID for every
semantic object reference. Its stable arguments are `target_kind`,
`preserve_existing`, and aligned `operation_kinds`, `object_kinds`, and
`object_uuids` lists. Updates preserve UUID ordering, ignore repeated adds,
remove matching UUIDs, and leave all non-target selection categories intact.
They set `selectionChanged` only for an actual ordered selection change and do
not create Undo state or dirty the project.

Console `f` and `t` numeric IDs are adapter syntax rather than semantic
identities. Fixture commands retain the prior fixture selection; matching the
historical Console behavior, each truss command starts its target category
empty before applying its ordered operations. Ranges and mixed add/remove
operations are resolved in parser order. Missing numeric IDs remain successful
no-ops with an information diagnostic. An ADD with duplicate fixture IDs or
truss unit numbers previously selected whichever unordered-map entry appeared
first; because that behavior was unsafe and nondeterministic, the adapter emits
`scene.selection.numeric_id_ambiguous` and skips the ADD. REMOVE instead
resolves against the ordered current target selection and removes every
selected object whose numeric ID matches, even when unselected scene objects
share that ID. Both operations produce only stable object-kind and UUID
references for semantic execution.

`scene.selection.clear` retains the narrowly characterized embedded Console
behavior: it clears fixtures, trusses, and scene objects but not supports. It
also retains the legacy `cli clear` Undo publication and dirty-state effect,
including when those three categories were already empty. The latter case is
reported as `scene.selection.clear_noop`; the Undo publication is compatibility
behavior rather than evidence of scene-content mutation. No general scene
reset behavior is implied.

Selection and clear commands mutate neither scene resources nor transforms.
Tables and viewers are synchronized only by the Console after each command.
Because the Console publishes the resulting selection before executing the
next parsed command, a transform later in the same line observes the selection
produced by every preceding command.

## Scene transform commands

The focused `perastage_command_transform` production target implements two
stable operations: `scene.transform.position`, whose component values and
ranges are explicitly millimeters, and `scene.transform.rotation`, whose
component values and ranges are explicitly degrees.

A typed command contains ordered X/Y/Z components. Each component retains its
single value or interpolation range, relative/absolute mode, explicit `world`
or `local` transform space, and the legacy group-pivot marker. An optional
pivot is an explicit XYZ millimeter triplet. The deterministic generic
`Request` projection uses axis-specific `_millimeters` or `_degrees` arguments
plus `_relative`, `_space`, and `_group` arguments, followed by `pivot_mm` when
present.

Execution uses only the scene and current `ObjectSelection` supplied through
`ExecutionContext`, plus an explicitly supplied `InteractiveTransformPolicy`.
Relative transforms use the existing transform-space helpers. Absolute
rotation retains the established Console Euler-axis mapping and preserves
origin and scale. Group rotation uses either its explicit pivot or the center
of the current effective targets' world origins.

Every semantic transform runs through one `MutationTransaction`. A changed
multi-axis command publishes one pre-mutation snapshot with the compatible
`cli pos` or `cli rot` Undo label. A tolerance-equivalent no-op succeeds with
an information diagnostic and publishes nothing. Publication failure restores
the exact scene and selection captured before execution.

The application-side mutation host converts neutral object selection to the
project selection snapshot and publishes it through `PushUndoSnapshot`; it
does not refresh GUI state. Frontends use `MutationSummary::sceneChanged` to
decide whether to refresh tables or viewers.

There are two Command mutation contexts. The embedded Console obtains the
active GUI scene and publishes each semantic mutation through application Undo
and dirty-state services. The external `perastage-cli scene` workflow instead
owns an isolated scene read from an explicitly named MVR, uses a headless
`ProjectMutationHost` with no persistent Undo stack, and publishes only to a
different, explicitly named output MVR after every command succeeds. Both
contexts use this same text processor and typed selection/transform Commands.
The local live CLI adapter executes this same text processor against the active
application scene and selection through `GuiProjectMutationHost`. Its dedicated
loopback transport contains no scene logic, and the application adapter returns
mutations through the existing main-window refresh boundary.

## Higher-level scene tools

The `perastage_command_scene_tools` target exposes four stable semantic IDs:

- `scene.group.create` and `scene.group.ungroup` take ordered
  `fixture_uuids`, `truss_uuids`, `support_uuids`, and
  `scene_object_uuids`. Creation outputs `group_uuid`; both operations output
  the four ordered `affected_*_uuids` arrays. Group creation requires at least
  two distinct typed identities; repeating a UUID within one object kind is a
  `scene.group.duplicate_object` validation error rather than an instruction to
  normalize the request.
- `scene.convert.fixture_to_support` takes ordered, unique `fixture_uuids` and
  outputs `converted_uuids` plus `cleared_motor_reference_uuids`.
- `scene.convert.scene_objects_to_trusses` takes one
  `source_scene_object_uuid`. Its intentionally explicit scope is every scene
  object with the source object's existing model identity; it outputs
  `converted_uuids` and `model_file`.

All identities are stable UUIDs and all arguments are explicit. Conversion is
destructive: it replaces the source node type while preserving UUID, placement,
hierarchy metadata, and the existing Core service's cross-reference behavior.
The commands validate their complete target scope before mutation, update the
semantic selection, and publish exactly one Undo snapshot. Mutation summaries
compare the ordered pre-command and post-command selections, so
`selectionChanged` is independent from `sceneChanged`. Same-model truss
conversion rejects the complete request before mutation if any target UUID is
already owned by a truss, and its diagnostic lists every conflicting UUID in
deterministic order. True no-ops publish nothing, and publication failure
restores scene and selection and returns no committed-result outputs.
Main-window handlers now only collect inputs, invoke these commands, present
results, and refresh frontend views.

### CMD-440 candidate audit

| Candidate | Existing owner and production caller | Inputs and implicit state | Mutation, determinism, and decision |
| --- | --- | --- | --- |
| Group / ungroup | `scene_grouping`; group and ungroup main-window actions | Cross-table typed UUID selection; no preference | Mutates GroupObjects, hierarchy, layers and local metadata, then replaces selection with affected UUIDs. Deterministic except newly generated group UUID. Migrated. |
| Add/remove group members | `scene_grouping`; drag/group interaction paths | Target group plus interaction-derived membership | The service is reusable, but its current production workflows do not form a standalone semantic command consumer. Deferred rather than creating an unused abstraction. |
| Fixture to Support | `scene_node_operations`; Convert to Hoist action | Explicit fixture UUIDs; no preference | Destructively replaces node kind, preserves UUID and hierarchy, clears motor references deterministically, and selects supports. Migrated. |
| Same-model SceneObject to Truss | scene-object truss converter; Convert Scene Objects action | Explicit source UUID; same-model scope is part of the established service | Destructively replaces all matching objects while preserving UUIDs; the adapter sorts converted UUID output and selection for deterministic ordering. Migrated. |
| Whole/selected auto patch | `AutoPatcher`; Auto Patch action and rider import | Selection chooses scope; GUI currently supplies implicit default universe/channel | Overwrites DMX addresses and has mature topology/order rules, but the service returns no mutation/result characterization and the importer is a second non-transactional consumer. Deferred pending a focused result boundary rather than guessing change detection in Command Core. |
| Line distribution | `fixture_line_distribution`; fixture-distribution dialogs and viewport actions | Ordered fixtures and line/spacing values, but edge extents and points are currently gathered by viewer/dialog workflows | Deterministic fixture transforms, with ordering significant. Deferred because not every exposed GUI variant has neutral geometry ownership; Command Core must not depend on viewer loaders or picking state. |

Grouping previously pushed Undo before attempting the operation and then called
Undo for a no-op. The semantic command intentionally produces the same final
scene/history state without the transient, meaningless history operation.
SceneObject conversion previously left a pre-pushed Undo entry when its source
had no valid model; validation now produces no Undo entry.

## Results and diagnostics

The four distinct outcomes are `success`, `parse_error`, `validation_error`,
and `execution_error`. Ordered diagnostics carry a severity (`information`,
`warning`, or `error`), phase (`parse`, `validation`, or `execution`), stable
code, technical-English message, and optional argument ID. Warnings may coexist
with success. A parse error may have no semantic request, represented as a
missing request in C++ and `null` in JSON.

Mutation summaries report only semantic facts: scene, selection, or project
metadata changes; whether an Undo entry was recorded; and whether the project
is dirty. Viewer, table, and other presentation refresh decisions belong to the
frontend and must not enter this contract.

The optional ordered `outputs` collection carries the same typed scalar and
homogeneous-array values as request arguments. It is reserved for immediate
machine-readable command results such as created or converted UUIDs, rather
than arbitrary recursive JSON.

## Execution and transaction boundary

`ExecutionContext` explicitly supplies an `MvrScene`, the shared
`scene_grouping::ObjectSelection`, and a neutral `ProjectMutationHost`. A host
publishes one captured pre-mutation scene/selection snapshot under a stable
English Undo label and reports Undo and dirty state. Command Core does not know
about `ConfigManager`, project-session implementation, panels, or viewers.

`MutationTransaction` captures scene and selection once. Destruction or an
explicit rollback restores both. A no-op also restores provisional changes and
does not call the host. A real commit calls the host exactly once and copies its
Undo/dirty response into the mutation summary. If publication throws, scene and
selection are restored and the caller converts the exception to an execution
error. Validation and other speculative work therefore cannot publish project
state.

## Machine schema

Command JSON has `schema_version` 1 and the top-level members
`schema_version`, `request`, `outcome`, `diagnostics`, `mutation`, and `outputs`. Arguments
retain their JSON scalar or homogeneous-array types and their in-memory order;
diagnostics also retain order. A parse failure without a request emits
`"request": null`.

Schema version 1 is additive within its lifetime: consumers must tolerate new
object members but can rely on documented members and tokens retaining their
meaning. A breaking removal, token change, type change, or semantic change
requires a new schema version.
