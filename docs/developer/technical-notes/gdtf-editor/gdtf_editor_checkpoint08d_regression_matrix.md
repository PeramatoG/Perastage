# GDTF Editor Checkpoint 08D Regression Matrix

Checkpoint 08D verifies transaction stability after Fixture and Truss hosts adopted the reusable GDTF editor panels and apply adapters.

## Fixture sources

| Source | Expected result |
| --- | --- |
| MVR-extracted GDTF | Physical edits create a Perastage derivative; the extracted source is not overwritten. |
| User Fixture library source | Physical edits create or reuse a stable Perastage derivative and preserve dictionary type context. |
| Existing Perastage derivative | Physical edits update the owned derivative instead of creating a derivative-of-derivative. |
| Missing source | Apply fails with one concise validation message and no project/table/cache mutation. |
| Unicode path | Metadata, modes, channels, mutation, and derivative references use UTF-8-safe path conversion. |

## Fixture edits

| Edit | Expected result |
| --- | --- |
| No-op | No undo entry, file write, table refresh, viewer rebuild, or dirty-state change. |
| Mode only | One project undo checkpoint, no derivative, committed mode reference, clean session baseline. |
| Source/type only | One project undo checkpoint and table/cache/session rebind to the resulting source. |
| Weight only | One project undo checkpoint; propagated fixtures in the same source/type family update. |
| Power only | One project undo checkpoint; original source is unchanged when a derivative is required. |
| Weight + Power | One project undo checkpoint and one physical write target. |
| Mixed project fields | Adapter preparation succeeds before table/project commit; all changes commit together. |
| Derivative failure | Dialog remains open; table, project, row cache, session, visual color, and dirty state remain unchanged. |
| Physical write failure | No project commit; only safe newly-created incomplete outputs may be cleaned up. |
| Reopen and second Apply | The editor reopens on the resulting derivative; no-op second Apply creates no derivative or undo. |
| Undo | Project Fixture values and references return to the previous project state; external files are outside undo. |

## Truss sources

| Source | Expected result |
| --- | --- |
| Geometry-only | Generation occurs only when a GDTF-owned value changes. |
| External/extracted GDTF | Generated project-controlled output is committed without resolving references against the process cwd. |
| Existing generated Perastage GDTF | Owned generated output may be updated; project references remain stable. |
| Missing resource | Required resource failure stops Apply before project/table/cache mutation. |
| Unicode resource path | Resource resolution and generated references preserve UTF-8 paths. |

## Truss edits

| Edit | Expected result |
| --- | --- |
| No-op | No generation, undo entry, table refresh, viewer rebuild, or dirty-state change. |
| MVR-only | One undo checkpoint through table synchronization; no GDTF generation. |
| Manufacturer/Model | One generation and one undo checkpoint. |
| Dimensions | One generation and one undo checkpoint. |
| Weight | One generation and one undo checkpoint. |
| Cross Section only | One generation and one explicit undo checkpoint even when no table column changes. |
| Mixed Apply | Adapter preparation succeeds before table/project commit; exactly one undo checkpoint. |
| Generation failure | Dialog remains open; table, project, session, and caches remain unchanged. |
| Reopen and second Apply | Metadata and preview use the generated result; no-op second Apply does not regenerate. |
| Undo | Project truss type values and generated references restore to the previous project state. |
