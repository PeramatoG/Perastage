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
findings are not duplicated there.

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
public distribution and the CLI-220 automation compatibility contract remain
future work.
