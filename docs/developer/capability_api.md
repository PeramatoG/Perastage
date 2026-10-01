# Capability discovery API

## Ownership and purpose

`core/capability/` owns the GUI-independent catalog of semantic Command and
Query operations that exist today. `perastage_capability_core` owns neutral C++
descriptors and structural Command request validation, while
`perastage_capability_serialization` owns deterministic JSON schema version 1.
The catalog is descriptive only: it contains no callbacks, scene access,
dispatch, confirmation policy, or transport behavior.

Stable Command IDs are owned by the neutral
`core/command/command_operation_ids.h` contract. Capability Core consumes that
header and the generic request value contract without linking Command execution,
mutation, or transaction targets. The serialization target depends only on
Capability Core, and the development CLI consumes that serialization facade.

Descriptors contain the stable operation ID, `command` or `query` kind, a
concise English summary, effect, arguments, and current frontend exposure.
Arguments use the closed vocabulary `boolean`, `int64`, `float64`, `string`,
`int64_list`, `float64_list`, and `string_list`. Required means unconditionally
required by the generic request shape; semantic constraints remain with the
operation.

Effects are `read_only`, `mutating`, and `destructive`. Destructive identifies
the conversions that replace scene node types; it does not prescribe a
confirmation policy. Selection clear is mutating, not destructive.

## Discovery and frontend exposure

`Catalog()` returns every descriptor once in ascending operation-ID order, and
`Find()` performs lookup without application state. Exposure uses only current
technical frontend identifiers: `embedded_console` and `desktop_gui`. `full`
means the frontend exposes the semantic surface; `partial` records a meaningful
subset. Console selection update is partial because its numeric grammar selects
fixtures and trusses while the semantic operation also accepts supports and
SceneObjects. An empty list means Core exists without a current frontend. The
development CLI has no execution exposure; `perastage-cli capabilities` only
reads the catalog.

The `scene.object.get` metadata names its existing typed C++ object reference as
`object_kind` and `object_uuid`. Patch footprint resolution is host context and
is not an argument.

## Adapter contract

Capability metadata is distinct from embedded Console grammar such as `f`,
`t`, `pos`, axis shorthand, and ranges. That human syntax remains in the text
adapters. Future FRONT-510, FRONT-515, FRONT-520, and FRONT-530 adapters should
consume this inventory rather than copy operation lists, while invoking Command
or Query services directly for behavior. They must not turn the catalog into an
execution registry.
