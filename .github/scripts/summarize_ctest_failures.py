#!/usr/bin/env python3
"""Create a concise CTest failure summary from JUnit, LastTestsFailed, and logs."""
from __future__ import annotations

import argparse
import csv
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Failure:
    platform: str
    test: str
    status: str
    first_failure_line: str
    source_log: str


LOG_FAILURE_RE = re.compile(
    r"^\s*\d+/\d+ Test\s+#?\d+:\s+(.+?)\s+\.{3,}\*\*\*(Failed|Exception|Timeout|SEGFAULT|Subprocess aborted)(.*)$",
    re.IGNORECASE,
)
START_RE = re.compile(r"^\s*Start\s+\d+:\s+(.+?)\s*$")
LAST_FAILED_RE = re.compile(r"^\s*\d+:\s*(.+?)\s*$")
NOISE_PREFIXES = (
    "Start ",
    "Test command:",
    "Working Directory:",
    "Environment variables:",
    "Test timeout computed to be",
)
NAMED_BLOCK_RE = re.compile(r"^.*?\b(BEGIN|END)\s+(\S+)\s*$")


# Returns the most recent named diagnostic block that did not reach its END marker.
def incomplete_named_block(lines: list[str]) -> str | None:
    open_blocks: list[tuple[str, str]] = []
    for line in lines:
        match = NAMED_BLOCK_RE.match(line.strip())
        if not match:
            continue
        marker, name = match.groups()
        if marker == "BEGIN":
            open_blocks.append((name, line.strip()))
            continue
        for index in range(len(open_blocks) - 1, -1, -1):
            if open_blocks[index][0] == name:
                del open_blocks[index]
                break
    return open_blocks[-1][1][:240] if open_blocks else None


# Returns the first diagnostic line that is likely to explain a failure.
def meaningful_line(text: str, fallback: str = "<no failure detail found>") -> str:
    preferred = (
        "Assertion",
        "assert",
        "Expected",
        "expected",
        "Actual",
        "actual",
        "Subprocess aborted",
        "Exception",
        "Segmentation fault",
        "SEGFAULT",
        "Timeout",
        "Failed",
        "Error",
    )
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    for needle in preferred:
        for line in lines:
            if needle in line:
                return line[:240]
    for line in lines:
        if not line.startswith(NOISE_PREFIXES):
            return line[:240]
    return fallback


# Reads CTest's LastTestsFailed.log names when the file exists.
def read_last_failed(path: Path | None) -> list[str]:
    if not path or not path.exists():
        return []
    names: list[str] = []
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        match = LAST_FAILED_RE.match(line)
        if match:
            names.append(match.group(1).strip())
    return names


# Extracts failed testcases from CTest JUnit XML.
def failures_from_junit(platform: str, junit: Path | None) -> dict[str, Failure]:
    failures: dict[str, Failure] = {}
    if not junit or not junit.exists():
        return failures
    try:
        root = ET.parse(junit).getroot()
    except ET.ParseError:
        return failures
    for testcase in root.iter("testcase"):
        failure_nodes = list(testcase.findall("failure")) + list(testcase.findall("error"))
        skipped_nodes = list(testcase.findall("skipped"))
        if not failure_nodes and not skipped_nodes:
            continue
        name = testcase.get("name") or testcase.get("classname") or "<unknown>"
        node = failure_nodes[0] if failure_nodes else skipped_nodes[0]
        status = node.get("message") or node.tag
        body = "\n".join(filter(None, [node.text or "", testcase.findtext("system-out") or "", testcase.findtext("system-err") or ""]))
        detail = meaningful_line(body, status)
        if "timeout" in status.lower():
            detail = incomplete_named_block([line.strip() for line in body.splitlines() if line.strip()]) or detail
        failures[name] = Failure(platform, name, status, detail, str(junit))
    return failures


# Extracts failed tests from a complete or partial CTest text log.
def failures_from_log(platform: str, log: Path | None) -> dict[str, Failure]:
    failures: dict[str, Failure] = {}
    if not log or not log.exists():
        return failures
    lines = log.read_text(encoding="utf-8", errors="ignore").splitlines()
    current_test = ""
    current_output: list[str] = []
    for index, line in enumerate(lines):
        start = START_RE.match(line)
        if start:
            current_test = start.group(1).strip()
            current_output = []
            continue
        if current_test:
            current_output.append(line)
        match = LOG_FAILURE_RE.match(line)
        if not match:
            continue
        name = match.group(1).strip()
        status = (match.group(2) + match.group(3)).strip()
        lookahead = "\n".join(lines[index + 1:index + 80])
        text = "\n".join(current_output + [lookahead])
        detail = meaningful_line(text, status)
        if "timeout" in status.lower():
            detail = incomplete_named_block(current_output) or detail
        failures[name] = Failure(platform, name, status, detail, str(log))
    if current_test and current_test not in failures:
        tail = "\n".join(current_output[-80:])
        if any(marker in tail for marker in ("Interrupted", "Cancel", "cancel", "Terminated")):
            failures[current_test] = Failure(platform, current_test, "interrupted", meaningful_line(tail, "interrupted"), str(log))
    return failures


# Writes a deterministic CSV summary.
def write_csv(path: Path, failures: list[Failure]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["platform", "test", "status", "first_failure_line", "source_log"])
        for failure in failures:
            writer.writerow([failure.platform, failure.test, failure.status, failure.first_failure_line, failure.source_log])


# Builds the merged summary, preserving every LastTestsFailed entry.
def summarize(args: argparse.Namespace) -> list[Failure]:
    merged = failures_from_junit(args.platform, Path(args.junit) if args.junit else None)
    log_failures = failures_from_log(args.platform, Path(args.log) if args.log else None)
    for name, failure in log_failures.items():
        merged.setdefault(name, failure)
    last_failed = read_last_failed(Path(args.last_tests_failed) if args.last_tests_failed else None)
    source = args.last_tests_failed or args.log or args.junit or "<unknown>"
    for name in last_failed:
        merged.setdefault(name, Failure(args.platform, name, "failed", "listed in LastTestsFailed.log", source))
    return [merged[name] for name in sorted(merged)]


# Parses command-line arguments and writes the summary.
def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", required=True)
    parser.add_argument("--junit")
    parser.add_argument("--log")
    parser.add_argument("--last-tests-failed")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    write_csv(Path(args.output), summarize(args))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
