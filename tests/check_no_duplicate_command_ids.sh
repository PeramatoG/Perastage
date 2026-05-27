#!/usr/bin/env bash
set -euo pipefail

# Detects duplicate resolved numeric values among MainWindow command IDs.
python3 - <<'PY'
import re
import sys
from collections import defaultdict
from pathlib import Path

# Extracts constexpr ID definitions from a header file.
def parse_definitions(path: Path):
    pattern = re.compile(r"^\s*constexpr\s+int\s+(ID_[A-Za-z0-9_]+)\s*=\s*(.+);\s*$")
    defs = {}
    current = ""
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not current and not line.startswith("constexpr int ID_"):
            continue
        current = f"{current} {line}".strip() if current else line
        if not current.endswith(";"):
            continue
        match = pattern.match(current)
        if match:
            defs[match.group(1)] = match.group(2).strip()
        current = ""
    return defs

# Resolves a single ID expression into an integer, supporting literals and + / - arithmetic over other IDs.
def resolve_value(symbol, definitions, resolved, resolving):
    if symbol in resolved:
        return resolved[symbol]
    if symbol in resolving:
        raise ValueError(f"Cyclic ID definition detected at {symbol}")
    if symbol not in definitions:
        raise ValueError(f"Unknown ID symbol referenced: {symbol}")

    resolving.add(symbol)
    expr = definitions[symbol]
    tokens = re.findall(r"ID_[A-Za-z0-9_]+|wxID_[A-Za-z0-9_]+|\d+|[+-]", expr)
    if not tokens:
        raise ValueError(f"Unsupported empty expression for {symbol}: {expr}")

    def token_value(token):
        if token.isdigit():
            return int(token)
        if token.startswith("ID_"):
            return resolve_value(token, definitions, resolved, resolving)
        if token == "wxID_HIGHEST":
            return 10000
        raise ValueError(f"Unsupported token {token} in expression for {symbol}: {expr}")

    total = token_value(tokens[0])
    i = 1
    while i < len(tokens):
        op = tokens[i]
        if i + 1 >= len(tokens):
            raise ValueError(f"Dangling operator in expression for {symbol}: {expr}")
        value = token_value(tokens[i + 1])
        if op == "+":
            total += value
        elif op == "-":
            total -= value
        else:
            raise ValueError(f"Unsupported operator {op} in expression for {symbol}: {expr}")
        i += 2

    resolved[symbol] = total
    resolving.remove(symbol)
    return total

# Resolves IDs and reports any duplicate numeric assignments.
def main():
    id_headers = sorted(Path("gui/mainwindow/ids").glob("*_ids.h"))
    if not id_headers:
        print("No ID headers found under gui/mainwindow/ids.", file=sys.stderr)
        return 1

    definitions = {}
    for header in id_headers:
        definitions.update(parse_definitions(header))

    resolved = {}
    for symbol in definitions:
        resolve_value(symbol, definitions, resolved, set())

    collisions = defaultdict(list)
    for symbol, value in resolved.items():
        collisions[value].append(symbol)

    duplicates = {value: names for value, names in collisions.items() if len(names) > 1}
    if duplicates:
        print("Duplicate command IDs detected:", file=sys.stderr)
        for value in sorted(duplicates):
            names = ", ".join(sorted(duplicates[value]))
            print(f"  value {value}: {names}", file=sys.stderr)
        return 1

    print("OK: no duplicate command IDs found in gui/mainwindow/ids/*.h.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
PY
