#!/usr/bin/env python3
"""Write compact machine-readable CTest result counts for CI diagnostics."""
from __future__ import annotations

import argparse
import json
import xml.etree.ElementTree as ET
from pathlib import Path


def parse_int(value: str | None) -> int:
    try:
        return int(value or 0)
    except ValueError:
        return 0


def counts_from_junit(path: Path) -> dict[str, int]:
    if not path.exists():
        return {"total": 0, "passed": 0, "failed": 0, "skipped": 0, "disabled_not_run": 0}
    root = ET.parse(path).getroot()
    total = parse_int(root.get("tests"))
    failed = parse_int(root.get("failures")) + parse_int(root.get("errors"))
    skipped = parse_int(root.get("skipped"))
    disabled = parse_int(root.get("disabled"))
    if total == 0:
        cases = list(root.iter("testcase"))
        total = len(cases)
        failed = sum(1 for case in cases if case.find("failure") is not None or case.find("error") is not None)
        skipped = sum(1 for case in cases if case.find("skipped") is not None)
    passed = max(total - failed - skipped - disabled, 0)
    return {"total": total, "passed": passed, "failed": failed, "skipped": skipped, "disabled_not_run": disabled}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", required=True)
    parser.add_argument("--junit", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--tested-sha", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--labels", default="")
    args = parser.parse_args()

    counts = counts_from_junit(Path(args.junit))
    result = {
        "platform": args.platform,
        "tested_sha": args.tested_sha,
        "profile": args.profile,
        "selected_labels": [label for label in args.labels.split(",") if label],
        **counts,
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
