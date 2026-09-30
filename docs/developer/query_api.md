# Scene and project Query API

Query Core is the structured, read-only counterpart to the Command API. A
headless adapter supplies an explicit active `MvrScene` and semantic selection;
no `ConfigManager`, window, table, viewer, wxWidgets lifecycle, or mutable
application service is part of the contract.

## Stable operations

| Identifier | Structured result |
|---|---|
| `scene.summary` | MVR version and fixture, truss, support, scene-object, group, and layer counts. |
| `scene.selection.get` | Typed UUID references from the current semantic selection. |
| `scene.objects.list` | Stable descriptors containing kind, UUID, name, layer, parent group, and selected type/resource facts. |
| `scene.object.get` | One descriptor matched by both kind and UUID, or `scene.object.not_found`. |
| `scene.layers.list` | Layer identity, name, color, and resolvable typed child membership. |
| `scene.groups.list` | Group identity, layer/parent relationship, and typed direct children. |
| `scene.patch.status` | Per-fixture parsed address and resolved range, overlaps, and diagnostics. |

Results are values rather than serialized internal model structs. Object and
selection records are ordered by stable technical kind token and then UUID.
Layers and groups are ordered by UUID, their membership uses the object rule,
and patch fixtures are ordered by UUID. Duplicate selection and layer
references are collapsed. Repeated queries over unchanged state therefore
produce identical results, independent of unordered-map insertion order.

## Patch diagnostics

Addresses must be complete positive `universe.channel` values with channels in
the range 1 through 512. The caller supplies a read-only footprint resolver,
normally backed by an authoritative GDTF read context. Query Core never loads
or downloads a GDTF and never substitutes a one-channel footprint. It reports
`scene.patch.invalid_address`, `scene.patch.unresolved_footprint`, and
`scene.patch.range_exceeds_universe`. Conflicts contain the intersecting
channel interval and two fixture UUIDs. This analysis does not change AutoPatch
behavior.

## Validation provenance

Inspection Core reports describe the exact source file or immutable bytes that
were inspected. They are intentionally not returned as active-scene facts
because an imported scene may have changed afterward. A future integration may
surface validation only when it can bind diagnostics to the current project
revision; it must reuse Inspection Core rather than create another validator.

Query operations never mutate scene or selection state, publish Undo, mark the
project dirty, or trigger presentation work. Human serialization and CLI,
Console, IPC, OSC, MCP, and AI adapters remain separate frontend work.
