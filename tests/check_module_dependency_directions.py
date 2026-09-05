#!/usr/bin/env python3
"""Enforce the accepted source-level dependency directions between modules."""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

MODULES = ("core", "models", "mvr", "gui", "viewer_common", "viewer2d", "viewer3d")
SOURCE_SUFFIXES = {".h", ".hpp", ".hh", ".hxx", ".c", ".cc", ".cpp", ".cxx"}
# This is a reviewed contract, not a graph generated or rewritten by this check.
ACCEPTED_DIRECTIONS: frozenset[tuple[str, str]] = frozenset({
    ("core", "models"), ("core", "mvr"), ("core", "viewer2d"), ("core", "viewer3d"),
    ("gui", "core"), ("gui", "models"), ("gui", "mvr"), ("gui", "viewer2d"),
    ("gui", "viewer3d"), ("gui", "viewer_common"),
    ("mvr", "core"), ("mvr", "gui"), ("mvr", "models"), ("mvr", "viewer2d"),
    ("mvr", "viewer3d"),
    ("viewer2d", "core"), ("viewer2d", "gui"), ("viewer2d", "models"),
    ("viewer2d", "viewer3d"), ("viewer2d", "viewer_common"),
    ("viewer3d", "core"), ("viewer3d", "gui"), ("viewer3d", "models"),
    ("viewer3d", "viewer2d"), ("viewer3d", "viewer_common"),
    ("viewer_common", "core"),
})
# Keep this order aligned with application target include-directory accumulation.
INCLUDE_ROOTS = (
    "core", "core/diagnostics", "core/layouts", "core/print",
    "gui", "gui/mainwindow/controllers", "gui/mainwindow/ids",
    "models", "mvr", "viewer2d", "viewer2d/pdf", "viewer3d",
    "viewer3d/interfaces", "viewer3d/resources", "viewer3d/culling",
    "viewer3d/labels", "viewer3d/picking", "viewer3d/render", "viewer_common",
)
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"\r\n]+)"', re.MULTILINE)


@dataclass(frozen=True)
class Evidence:
    consumer: str
    provider: str
    source: Path
    spelling: str
    resolved: Path


def production_files(root: Path) -> list[Path]:
    """Return audited C and C++ files owned by the seven production modules."""
    return sorted(
        path
        for module in MODULES
        for path in (root / module).rglob("*")
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES
        and "tests" not in path.relative_to(root / module).parts
    )


def quoted_includes(text: str) -> list[str]:
    """Extract quoted preprocessor includes while ignoring commented directives."""
    cleaned: list[str] = []
    index = 0
    in_block = False
    while index < len(text):
        if in_block:
            end = text.find("*/", index)
            if end < 0:
                cleaned.append("\n" * text[index:].count("\n"))
                break
            cleaned.append("\n" * text[index:end + 2].count("\n"))
            index = end + 2
            in_block = False
        elif text.startswith("/*", index):
            in_block = True
            index += 2
        elif text.startswith("//", index):
            end = text.find("\n", index)
            if end < 0:
                break
            cleaned.append("\n")
            index = end + 1
        else:
            cleaned.append(text[index])
            index += 1
    return INCLUDE_RE.findall("".join(cleaned))


def owner(root: Path, path: Path) -> str | None:
    """Map a resolved repository path to its top-level production owner."""
    try:
        first = path.resolve().relative_to(root.resolve()).parts[0]
    except (ValueError, IndexError):
        return None
    return first if first in MODULES else None


def repository_relative_path(root: Path, path: Path) -> Path:
    """Return a path relative to consistently resolved repository roots."""
    return path.resolve().relative_to(root.resolve())


def repository_path_for_display(root: Path, path: Path) -> str:
    """Render a repository-relative path consistently on every platform."""
    if not path.is_absolute():
        return path.as_posix()
    try:
        return repository_relative_path(root, path).as_posix()
    except ValueError:
        return path.resolve().as_posix()


def resolve_include(root: Path, source: Path, spelling: str) -> tuple[Path | None, list[Path]]:
    """Resolve an include using source-relative and ordered application include roots."""
    relative = (source.parent / spelling).resolve()
    if relative.is_file():
        return relative, []

    candidates: list[Path] = []
    for include_root in INCLUDE_ROOTS:
        candidate = (root / include_root / spelling).resolve()
        if candidate.is_file() and owner(root, candidate):
            candidates.append(candidate)
    unique = list(dict.fromkeys(candidates))
    if len(unique) == 1:
        return unique[0], []
    if len(unique) > 1:
        return None, unique

    parts = Path(spelling).parts
    if parts and parts[0] in MODULES:
        qualified = (root / spelling).resolve()
        if qualified.is_file():
            return qualified, []
        return None, [qualified]
    return None, []


def audit(root: Path) -> tuple[list[Evidence], list[str]]:
    """Build the current cross-module include inventory and resolution errors."""
    root = root.resolve()
    evidence: list[Evidence] = []
    errors: list[str] = []
    for source in production_files(root):
        source_relative = repository_relative_path(root, source)
        consumer = source_relative.parts[0]
        for spelling in quoted_includes(source.read_text(encoding="utf-8", errors="replace")):
            resolved, conflicts = resolve_include(root, source, spelling)
            if conflicts:
                label = "Ambiguous" if len(conflicts) > 1 else "Unresolved project-local"
                rendered = ", ".join(repository_path_for_display(root, path) for path in conflicts)
                errors.append(
                    f'{label} include:\n  source: {repository_path_for_display(root, source)}\n'
                    f'  include: "{spelling}"\n  candidates: {rendered}'
                )
                continue
            if resolved is None:
                continue
            provider = owner(root, resolved)
            if provider and provider != consumer:
                evidence.append(
                    Evidence(consumer, provider, source_relative, spelling, repository_relative_path(root, resolved))
                )
    return evidence, errors


def validate(root: Path, accepted: frozenset[tuple[str, str]]) -> tuple[list[Evidence], list[str]]:
    """Compare the audited graph with the explicitly reviewed direction contract."""
    evidence, errors = audit(root)
    by_edge: dict[tuple[str, str], list[Evidence]] = defaultdict(list)
    for item in evidence:
        by_edge[(item.consumer, item.provider)].append(item)
    for edge in sorted(set(by_edge) - accepted):
        item = by_edge[edge][0]
        errors.append(
            f"Unexpected module dependency:\n  {item.consumer} -> {item.provider}\n"
            f"  introduced by: {repository_path_for_display(root, item.source)}\n"
            f"  include: \"{item.spelling}\"\n"
            f"  resolved to: {repository_path_for_display(root, item.resolved)}\n"
            "Update architecture documentation and the accepted "
            "dependency contract only if this direction is intentional."
        )
    missing = sorted(accepted - set(by_edge))
    for consumer, provider in missing:
        errors.append(
            f"Stale accepted module dependency: {consumer} -> {provider}\n"
            "Remove it from the contract and architecture documentation after review."
        )
    return evidence, errors


def documentation_contract_errors(root: Path) -> list[str]:
    """Require architecture documentation to describe the checked-in edge set."""
    path = root / "docs/developer/architecture.md"
    if not path.is_file():
        return ["Missing module dependency contract in docs/developer/architecture.md"]
    documented: set[tuple[str, str]] = set()
    rows = re.findall(r"^\| `([^`]+)` \| ([^|]+) \|", path.read_text(encoding="utf-8"), re.MULTILINE)
    consumers = {consumer for consumer, _ in rows if consumer in MODULES}
    for consumer, providers in rows:
        if consumer not in MODULES:
            continue
        for provider in re.findall(r"`([^`]+)`\s*\(\d+\)", providers):
            if provider in MODULES:
                documented.add((consumer, provider))
    errors: list[str] = []
    if consumers != set(MODULES):
        errors.append("Architecture dependency table must contain exactly the seven production modules")
    if documented != set(ACCEPTED_DIRECTIONS):
        errors.append(
            "Architecture dependency table and ACCEPTED_DIRECTIONS differ: "
            f"documented-only={sorted(documented - set(ACCEPTED_DIRECTIONS))}, "
            f"checker-only={sorted(set(ACCEPTED_DIRECTIONS) - documented)}"
        )
    return errors


def main() -> int:
    """Run the module direction policy check for a repository root."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--inventory", action="store_true", help="print the discovered edge counts")
    args = parser.parse_args()
    evidence, errors = validate(args.root.resolve(), ACCEPTED_DIRECTIONS)
    errors.extend(documentation_contract_errors(args.root.resolve()))
    if args.inventory:
        counts = Counter((item.consumer, item.provider) for item in evidence)
        for (consumer, provider), count in sorted(counts.items()):
            print(f"{consumer} -> {provider}: {count}")
    if errors:
        print("Module dependency direction check failed:", file=sys.stderr)
        for error in errors:
            print(f"\n{error}", file=sys.stderr)
        return 1
    print("Module dependency direction check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
