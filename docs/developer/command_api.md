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

The embedded Console consumes this neutral parser and retains its current
scene-dependent execution, selection resolution, Undo publication, and GUI
refresh responsibilities. CMD-420 and CMD-430 will migrate that execution and
selection behavior to concrete semantic commands; the text parser does not
prematurely define those contracts.

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
