# Developer CLI

Perastage provides a dedicated, headless command-line executable for development
and automated testing. Its CMake target is `perastage_cli`; the produced binary
is `perastage-cli` (`perastage-cli.exe` on Windows). It is not installed,
staged, packaged, or shipped yet.

## Grammar

The stable technical-English interface accepts these forms:

```text
perastage-cli -h
perastage-cli --help
perastage-cli --version
perastage-cli inspect --help
perastage-cli inspect <file>
perastage-cli inspect <file> --view summary
perastage-cli inspect <file> --view inventory
perastage-cli inspect <file> --view resources
perastage-cli inspect <file> --view diagnostics
perastage-cli inspect <file> --view xml
perastage-cli inspect <file> --json
perastage-cli capabilities [--json]
perastage-cli scene <input.mvr> --output <output.mvr> --command <text> [--command <text> ...] [--overwrite] [--json]
perastage-cli live command <text> [--port <port>]
perastage-cli live query <scene.summary|scene.selection.get> [--port <port>]
```

The `scene` command is the first external mutation workflow. Its input is a
read-only MVR container and its output is mandatory, explicit, and must resolve
to a different file. An existing output is rejected unless `--overwrite` is
present. Repeated commands execute in source order through the shared Console
text processor; only fixture/truss selection, `clear`, and position/rotation
transforms currently form this surface. A parse, validation, execution,
resource, serialization, or publication failure leaves both the source and any
pre-existing output unchanged. Successful output uses the production canonical
MVR preparation, resource collection, XML serialization, and archive writer.
Application-owned exporters explicitly supply their resolved fixture fallback
through `MvrExportEnvironment`; the neutral CLI export supplies no fallback and
never searches Perastage's installed or user fixture libraries.

Unlike the embedded Console, which mutates the active GUI project and records
application Undo state, this workflow uses an isolated headless scene and has
no persistent interactive Undo stack. It does report dirty semantic mutations
through the normal Command transaction contract. It neither attaches to nor
controls a running Perastage process.

`live` connects to an already-running Perastage instance using bounded,
newline-framed JSON schema version 1 over IPv4 TCP bound strictly to
`127.0.0.1`; the default port is `49155`. Commands use the existing Console
text adapter and Command transaction, Undo, validation, dirty-state, and
frontend refresh boundaries against the active project and selection. The
initial query surface contains only `scene.summary` and `scene.selection.get`.
Unsupported operations receive a structured error. The endpoint provides no
discovery, LAN listener, remote access, filesystem, shell, MVR-xchange, OSC,
MCP, or TLS surface. Transport, protocol, unsupported-operation, and semantic
command failures return exit code `4`; code `3` remains reserved for fatal
inspection input.

`inspect` accepts exactly one filesystem input with a case-insensitive `.gdtf`
or `.mvr` extension. Its default view is `summary`. `--json` and `--view` are
mutually exclusive, may appear only once, and options follow the input file.
Unknown options or views, missing view values, and extra positional arguments
are usage errors. Shell-quoted paths containing spaces and Unicode filesystem
paths are passed directly through the standard C++ filesystem boundary.

## Automation contract

The CLI is development-only today, but automation should use the executable
name `perastage-cli`, the `inspect` command, and the
`inspect <file> --json` form. For that machine-facing form, the compatibility
surface is:

- the command identity and argument roles shown above;
- the JSON field names, value types, enum tokens, diagnostic codes, and
  meanings declared stable by the Inspection API;
- the top-level `schema_version`, `request`, `success`, `has_findings`,
  `worst_severity`, `diagnostics`, `format`, `status`, `package`, `resources`,
  and `validation` members, plus the format-specific `document` (GDTF) or
  `snapshot` (MVR) member;
- the exit codes and stdout/stderr responsibilities below;
- UTF-8 JSON strings and lossless Unicode filesystem-path round trips; and
- diagnostic locations, when Core supplies them: `source_path` identifies the
  inspected filesystem input, `package_entry` identifies an archive member,
  `xml_path` identifies a location within XML, and `line`/`column` provide
  one-based parser coordinates when available.

Consumers must select behavior by `schema_version` and should ignore unknown
object members so additive fields remain compatible. Schema version `1` is the
current contract. Within one schema version, existing fields and diagnostic
location members keep their names, JSON types, token meanings, and semantics;
new optional fields may be added. Removing or renaming a field, changing its
type, or reinterpreting its meaning incompatibly requires a new schema version.
Array order is contractual only where the Inspection API describes the data as
ordered; JSON object member order and serialized whitespace are not.

Human `--view` output is presentation-only. Headings, whitespace, alignment,
prose, and layout may change and must not be parsed by automation. Help and
version output formatting is likewise presentation-only unless a token is
explicitly added to this section as stable. Exact XML output is a retained
source payload for inspection, not a versioned Perastage data schema.
The human GDTF summary labels its semantic result as `Read status`; this is
independent from the separately displayed XML and schema validation statuses.
The JSON member remains `status` with its existing values and schema version.

For complete GDTF and MVR reports, `success` means that the read operation
produced the format-specific structured `document` or `snapshot`; it does not
claim standards conformance. `has_findings` is true when any base or validation
diagnostic is warning, error, or fatal, and `worst_severity` is the greatest
severity across both collections (including information). The top-level
`diagnostics` array continues to contain only base inspection diagnostics;
validation findings remain in their original `validation` layers so XML
well-formedness, schema conformance, and semantic/interoperability provenance
stay distinct. Thus a parseable schema-invalid file has `success: true`,
`has_findings: true`, an aggregate `worst_severity` of `error`, and exit code
`1`. An unreadable file has `success: false` and exit code `3`.

MVR validation is version-aware. MVR 1.6 uses the pinned official 1.6 XSD and
the separate parser-level 1.6 semantic rules. No authoritative MVR 1.5 XSD is
available in the pinned upstream schema history, so an MVR 1.5 report truthfully
marks the schema layer `unavailable` and identifies the pinned specification
revision used for parser-level 1.5 semantic checks; that Markdown specification
revision is not an XSD. Unsupported versions instead identify that no
applicable specification is pinned and leave `source_revision` empty. MVR 1.5
is not tested against the incompatible structural expectations of the 1.6 XSD.
The focused MVR 1.5 Support check currently proves required `ChainLength`
element presence; numeric value-domain validation remains unavailable without
an applicable official schema.

When the CLI is publicly released, a published machine-facing CLI/schema
contract will not silently remove, rename, type-change, or semantically
reinterpret existing behavior. Additive JSON fields remain permitted under the
rules above; incompatible JSON changes require an explicit schema-version
change. Incompatible command or exit-code changes require an intentional
compatibility decision with corresponding documentation and regression tests.
This policy establishes the compatibility baseline without treating the
currently uninstalled binary as already shipped or promising stability for
presentation text.

## Inspection views

- `summary` renders compact format-specific document or scene facts, package
  and resource totals, validation status, and diagnostic severity counts.
- `inventory` renders the ordered Core `PackageInventory`, including entry
  type, safe display path, known size, and path-safety state.
- `resources` renders ordered Core `ResourceDescriptor` values, including
  kind, known size, supported operations, and path safety. It does not decode
  images or models.
- `diagnostics` renders semantically unique top-level and validation-layer
  findings with severity, classification, domain, stable code, message, and
  available location. It keeps standards and compatibility findings distinct.
  Identical findings repeated for validation provenance are shown and counted
  once in human views; the complete JSON retains every original layer member.
- `xml` writes only the exact retained `description.xml` or
  `GeneralSceneDescription.xml` payload. It adds no heading or newline and
  performs no parsing, rewriting, or pretty-printing.
- `--json` writes one complete schema-version-1 report. The Core
  `perastage_inspection_report_serialization` boundary composes format status,
  package, resources, validation, and GDTF document or MVR snapshot facts over
  the minimal base result serialization. The base `SerializeResultToJson(Result)`
  contract remains unchanged.
  MVR snapshots add deterministic `focus_points`, `video_screens`, and
  `projectors` collections alongside the existing scene-node collections.

Both presentation modes consume the same single in-memory result returned by
`InspectGdtf` or `InspectMvr`. Resource metadata comes from the neutral resource
inspection API, whose default filesystem overload owns the bounded sniff limit.
No CLI formatter parses package or XML semantics.

## Exit codes and streams

| Code | Meaning |
| --- | --- |
| `0` | Successful clean inspection, scene publication, or help/version command; information-only findings are clean. |
| `1` | Inspection completed with non-fatal warning or error findings, including compatibility findings. |
| `2` | Malformed CLI usage. |
| `3` | A supported input could not be usefully inspected because of fatal input, package, or XML failure. |
| `4` | Unsupported inspection/live input, or a scene/live command/export/publication failure. |
| `5` | Unexpected internal CLI failure. |

Requested data (human views, exact XML, and JSON) goes to standard output.
Structured JSON remains available there for a supported malformed source when
Core returns diagnostics. Standard error is reserved for usage errors,
unsupported types, and concise unexpected process failures; normal inspection
findings are not duplicated there. Automation should interpret the process exit
code before inspecting output; warning/error reports at code `1` and structured
fatal reports at code `3` can still contain valid JSON on standard output.

## Architecture and distribution

The standalone Rust MCP adapter under `tools/mcp/` is a process consumer of
this machine interface. It invokes the executable directly with argv, consumes
JSON and exit codes, and does not link against CLI, Core, GUI, format parsing,
or local-IPC implementation. See [MCP adapter](mcp_adapter.md) for its typed
tool surface and stdio startup contract.

`cli/main.cpp` only adapts process arguments and invokes the independently
testable standard-C++ runner. CLI sources depend only on public Core inspection
headers and link the focused GDTF, MVR, resource, and report-serialization
inspection targets. The semantic work stays in Inspection Core, including its private or
transitive reader dependencies. CLI code does not include or directly call MVR
or GDTF reader implementations. The neutral external-scene workflow composes
the acquired-package reader, shared Command text processor, headless mutation
host, and canonical MVR exporter; CLI code owns only grammar, output policy,
and result formatting.

The command starts no `wxApp`, creates no window, and initializes no App, GUI,
viewer, ConfigManager, localization, project state, networking, library, or
credential service. The executable remains a Windows console program and a
non-bundle macOS executable. It deliberately has no install or packaging rule;
public distribution remains future work. Because the CLI never owns or loads
credential storage, user-secret services, or networking state, those contents
cannot enter its output; adding such a dependency is forbidden rather than
handled by output scrubbing. Reports expose the requested source path and
archive/XML locations supplied by Inspection Core, but not private temporary
workspace paths used by implementations.
