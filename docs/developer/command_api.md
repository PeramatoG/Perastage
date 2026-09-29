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

The embedded Console consumes this neutral parser. Its focused transform
adapter converts position values from the Console grammar's meters to semantic
millimeters, then invokes `perastage_command_transform`. The Console owns
project-context acquisition, structured-result presentation, and GUI refresh;
it does not own reusable transform algorithms. Selection and `clear` execution
remain Console-owned pending CMD-430. In particular, the parser records ordered
selection operations but does not decide whether they replace or extend the
current selection.

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

The public `perastage-cli` remains read-only. External transform syntax is
deferred until CMD-430 provides stable selectors and FRONT-510 provides
explicit input/output ownership; the semantic executor itself is headless and
does not depend on that future frontend.

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
`schema_version`, `request`, `outcome`, `diagnostics`, and `mutation`. Arguments
retain their JSON scalar or homogeneous-array types and their in-memory order;
diagnostics also retain order. A parse failure without a request emits
`"request": null`.

Schema version 1 is additive within its lifetime: consumers must tolerate new
object members but can rely on documented members and tokens retaining their
meaning. A breaking removal, token change, type change, or semantic change
requires a new schema version.
