# MCP adapter

## Scope and ownership

`tools/mcp/` contains a small standalone Rust server built with the official
`rmcp` SDK. It is an external tools adapter, not a Perastage application
module. It must not link to or import Perastage Core, GUI, scene models,
MVR/GDTF readers, or local-IPC implementation. Instead, it launches the stable
`perastage-cli` executable directly with an argv vector and consumes its JSON,
exit status, and diagnostic stream. No request is passed through a shell.

The adapter exposes MCP tools only. It has no prompts, resources, AI behavior,
authentication, network listener, HTTP transport, or general command tool.
`tests/check_mcp_adapter_boundary.sh` protects these constraints. Perastage
continues to own inspection, validation, queries, mutation transactions, Undo,
dirty-state handling, and GUI refresh behavior.

## Tool surface

| MCP tool | Effect | Perastage CLI mapping |
| --- | --- | --- |
| `discover_capabilities` | Read-only | `capabilities --json` |
| `inspect_mvr` | Read-only | `inspect PATH --json`, restricted to `.mvr` |
| `inspect_gdtf` | Read-only | `inspect PATH --json`, restricted to `.gdtf` |
| `live_scene_summary` | Read-only | `live query scene.summary` |
| `live_current_selection` | Read-only | `live query scene.selection.get` |
| `live_objects_list` | Read-only | `live query scene.objects.list` |
| `live_object_get` | Read-only | `live query scene.object.get --args '{"kind":"fixture","uuid":"…"}'` |
| `live_layers_list` | Read-only | `live query scene.layers.list` |
| `live_groups_list` | Read-only | `live query scene.groups.list` |
| `live_selection_update` | Selection-only | `live execute scene.selection.update --args '<typed arguments>'` |
| `live_selection_clear` | Mutating | `live command clear` |
| `live_position_transform` | Mutating | Typed single-axis millimetre values translated to `pos` syntax |
| `live_rotation_transform` | Mutating | Typed single-axis degree values translated to `rot` syntax |
| `live_batch_transform` | Mutating | `live execute scene.transform.batch --args '<aligned typed lists>'` |

Selection transforms require an axis, value, relative/absolute choice,
world/local space, and group choice. Batch transforms require explicit typed
targets and ordered components. The optional port is the only endpoint setting. The
adapter does not supply an implicit selection, infer a transform, accept
free-form Console text, or expose destructive conversion/grouping operations.
Mutation requests therefore continue through the running application's normal
semantic command validation and mutation publication path.

`live_batch_transform` takes `targets`, an ordered non-empty array of
`{kind, uuid, components}` entries. Each target's non-empty `components` array
contains `{kind, axis, value, mode, space}` entries. Target kinds are `fixture`,
`truss`, `support`, `scene_object`, and `group`; component kinds are `position`
and `rotation`. Position values are millimetres and rotation values are
degrees. Every component explicitly chooses an `x`, `y`, or `z` axis, an
`absolute` or `relative` mode, and `world` or `local` space. Local axes affect
relative operations; absolute components retain the existing transform
semantics. Repeated targets and components retain their input order.

The adapter checks non-empty target/component lists and UUIDs, typed enum
tokens, and finite values. It flattens target components in order into
`target_kinds`, `target_uuids`, `component_kinds`, `axes`, `values`, `modes`, and
`spaces`, then passes that JSON as a single argv value to the allowlisted
`scene.transform.batch` operation. It performs no scene lookup, mutation, or
layout calculations. Object existence, kind matches, preview execution, and
atomic publication remain owned by Perastage. The existing selection transform
tools retain their original mappings and behavior.

An explicit child target is never promoted to its parent or root group. An
explicit `group` target uses the existing descendant synchronization behavior.
The batch preserves the current selection. A rejected batch changes nothing;
a successful scene change creates one Undo entry and publishes once. A
semantic no-op creates no Undo entry or dirty state. Clients calculate any
desired layout themselves and submit the resulting coordinates.

Object lookup accepts all Query kinds, including `group`. Typed selection
update accepts `fixture`, `truss`, `support`, and `scene_object`. Its required
input is `target_kind`, `preserve_existing`, and ordered `operations`, each
containing an `add` or `remove` kind and explicit `{kind, uuid}` objects.
Every object must belong to the target category and exist in the scene.
`preserve_existing: false` replaces only that category before applying the
operations; `true` adds/removes from its current selection. Empty operations
with `false` clear that category. Other categories retain their selection.

The adapter flattens operations in order to the Command Core argument lists
`operation_kinds`, `object_kinds`, and `object_uuids`, alongside `target_kind`
and `preserve_existing`, then invokes `live execute` with `--args` as one argv
value. This uses the generic versioned local-live machine contract and the
existing semantic selection Command. Selection update creates no scene change,
Undo entry, or dirty state; normal application publication synchronizes tables,
2D/3D highlights, and Layout Viewer selection. Legacy `live_selection_clear`
retains its existing Console-compatible behavior.

Object discovery returns Query Core descriptors, including stored fixture IDs
and patch addresses when applicable. Clients filter those values themselves;
MCP implements no object lookup, classification, or filtering logic.

Each call returns a structured object containing `success`, `exit_code`, the
CLI `result`, and `stderr`. Structured Perastage results and diagnostics are
preserved even when the CLI returns a non-zero status. An unavailable CLI or
live endpoint is reported as an MCP tool error with structured adapter or CLI
diagnostics rather than being mistaken for an empty result.

## Batch transform example

Discover UUIDs with `live_objects_list` or `live_groups_list`, then replace the
example UUIDs with explicit targets in the current scene. Positions are in
millimetres and rotations in degrees. Submit this payload to
`live_batch_transform`:

```json
{
  "targets": [
    {
      "kind": "fixture",
      "uuid": "fixture-a-uuid",
      "components": [
        {"kind": "position", "axis": "x", "value": -2000, "mode": "absolute", "space": "world"},
        {"kind": "position", "axis": "y", "value": 1500, "mode": "absolute", "space": "world"}
      ]
    },
    {
      "kind": "fixture",
      "uuid": "fixture-b-uuid",
      "components": [
        {"kind": "position", "axis": "x", "value": 2000, "mode": "absolute", "space": "world"},
        {"kind": "rotation", "axis": "z", "value": 45, "mode": "relative", "space": "local"}
      ]
    }
  ]
}
```

## Build and stdio startup

Build and test independently from the native application:

```sh
cd tools/mcp
cargo build --locked
cargo test --locked
```

Start the server on standard input/output with the CLI available on `PATH`:

```sh
tools/mcp/target/debug/perastage-mcp
```

Alternatively, set `PERASTAGE_CLI` to the exact executable path. MCP protocol
messages use stdin/stdout; the child CLI's streams are captured internally and
never corrupt the MCP transport. For live tools, start Perastage normally so
its loopback-only live endpoint is available. A client may provide a specific
live port, otherwise the existing CLI default is used.

The pinned `Cargo.lock` resolves `rmcp` 3.x, whose stable protocol support
includes MCP 2026-07-28. Dependency upgrades should retain stdio-only features
and rerun the adapter contract and architecture-boundary checks. Hosted Debug
CI runs `cargo fmt --manifest-path tools/mcp/Cargo.toml --check` and
`cargo test --manifest-path tools/mcp/Cargo.toml --locked`; the workflow
architecture guard protects that coverage.
