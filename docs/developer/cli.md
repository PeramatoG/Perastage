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
```

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
- the top-level `schema_version`, `request`, `success`, `worst_severity`,
  `diagnostics`, `format`, `status`, `package`, `resources`, and `validation`
  members, plus the format-specific `document` (GDTF) or `snapshot` (MVR)
  member;
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
- `diagnostics` renders top-level and validation-layer findings with severity,
  classification, domain, stable code, message, and available location. It
  keeps standards and compatibility findings distinct.
- `xml` writes only the exact retained `description.xml` or
  `GeneralSceneDescription.xml` payload. It adds no heading or newline and
  performs no parsing, rewriting, or pretty-printing.
- `--json` writes one complete schema-version-1 report. The Core
  `perastage_inspection_report_serialization` boundary composes format status,
  package, resources, validation, and GDTF document or MVR snapshot facts over
  the minimal base result serialization. The base `SerializeResultToJson(Result)`
  contract remains unchanged.

Both presentation modes consume the same single in-memory result returned by
`InspectGdtf` or `InspectMvr`. Resource metadata comes from the neutral resource
inspection API, whose default filesystem overload owns the bounded sniff limit.
No CLI formatter parses package or XML semantics.

## Exit codes and streams

| Code | Meaning |
| --- | --- |
| `0` | Successful clean inspection or successful help/version command; information-only findings are clean. |
| `1` | Inspection completed with non-fatal warning or error findings, including compatibility findings. |
| `2` | Malformed CLI usage. |
| `3` | A supported input could not be usefully inspected because of fatal input, package, or XML failure. |
| `4` | Unsupported input extension/type. |
| `5` | Unexpected internal CLI failure. |

Requested data (human views, exact XML, and JSON) goes to standard output.
Structured JSON remains available there for a supported malformed source when
Core returns diagnostics. Standard error is reserved for usage errors,
unsupported types, and concise unexpected process failures; normal inspection
findings are not duplicated there. Automation should interpret the process exit
code before inspecting output; warning/error reports at code `1` and structured
fatal reports at code `3` can still contain valid JSON on standard output.

## Architecture and distribution

`cli/main.cpp` only adapts process arguments and invokes the independently
testable standard-C++ runner. CLI sources depend only on public Core inspection
headers and link the focused GDTF, MVR, resource, and report-serialization
inspection targets. The semantic work stays in Inspection Core, including its private or
transitive reader dependencies. CLI code does not include or directly call MVR
or GDTF reader implementations.

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
