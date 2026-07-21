#!/usr/bin/env python3
"""Create a concise summary from verbose CTest logs."""
from __future__ import annotations

import argparse
import re
from pathlib import Path

FAIL_RE = re.compile(r"^\s*\d+/\d+ Test\s+#?\d+:\s+(.+?)\s+\.{3,}\*\*\*Failed\s+(.+)$")


def meaningful(lines: list[str], start: int) -> str:
    for line in lines[start:start + 80]:
        stripped = line.strip()
        if stripped and not stripped.startswith(('Start ', 'Test command:', 'Working Directory:')):
            return stripped[:240]
    return '<no failure detail found>'


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--platform', required=True)
    parser.add_argument('--log', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    log = Path(args.log)
    lines = log.read_text(encoding='utf-8', errors='ignore').splitlines() if log.exists() else []
    rows = ['platform,test,status,first_failure_line,source_log']
    for index, line in enumerate(lines):
        match = FAIL_RE.match(line)
        if match:
            rows.append(','.join('"' + value.replace('"', '""') + '"' for value in [args.platform, match.group(1).strip(), match.group(2).strip(), meaningful(lines, index + 1), str(log)]))
    Path(args.output).write_text('\n'.join(rows) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
