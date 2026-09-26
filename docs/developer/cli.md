# Developer CLI

Perastage provides a dedicated, headless command-line shell for development and
automated testing. Its CMake target is `perastage_cli`; the produced executable
is named `perastage-cli` (`perastage-cli.exe` on Windows). This executable is
not installed, staged, packaged, or shipped yet.

## CLI-200 grammar

The technical interface is stable English and accepts only these forms:

| Arguments | Standard output | Exit code |
| --- | --- | --- |
| none | Help text | `0` |
| `-h` | Help text | `0` |
| `--help` | Help text | `0` |
| `--version` | `Perastage CLI <PROJECT_VERSION>` | `0` |

An unknown option, an unknown positional argument, or any extra argument after
`--help` or `--version` writes a stable usage diagnostic to standard error and
returns `2`. Successful commands return `0`. No inspection-specific exit codes
are defined at this stage.

## Architecture and distribution

`cli/main.cpp` only adapts process arguments and invokes the independently
testable standard-C++ runner. The `cli` module may depend on `core`, but it must
not depend on App, GUI, model, MVR, viewer, localization, configuration, project
state, or wxWidgets application lifecycle code. Help and version handling
therefore starts no `wxApp`, creates no window, and creates no user state.

The executable is a normal console program on Windows and a non-bundle command
on macOS. Its CMake target deliberately has neither `WIN32_EXECUTABLE` nor
`MACOSX_BUNDLE` enabled. Architecture checks and a target-resolved subprocess
smoke test enforce these properties on supported CI hosts.

The `inspect` command, human and JSON inspection output, and inspection-specific
exit rules belong to CLI-210 and are intentionally not part of CLI-200.
