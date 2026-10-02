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
| `live_selection_clear` | Mutating | `live command clear` |
| `live_position_transform` | Mutating | Typed single-axis millimetre values translated to `pos` syntax |
| `live_rotation_transform` | Mutating | Typed single-axis degree values translated to `rot` syntax |

Every transform requires an axis, value, relative/absolute choice, world/local
space, and group choice. The optional port is the only endpoint setting. The
adapter does not supply an implicit selection, infer a transform, accept
free-form Console text, or expose destructive conversion/grouping operations.
Mutation requests therefore continue through the running application's normal
live command validation and mutation publication path.

Each call returns a structured object containing `success`, `exit_code`, the
CLI `result`, and `stderr`. Structured Perastage results and diagnostics are
preserved even when the CLI returns a non-zero status. An unavailable CLI or
live endpoint is reported as an MCP tool error with structured adapter or CLI
diagnostics rather than being mistaken for an empty result.

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
and rerun the adapter contract and architecture-boundary checks.
