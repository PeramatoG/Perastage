#!/usr/bin/env python3
"""Validate local documentation links and website markdown targets."""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"

failures: list[str] = []


def require_text(path: pathlib.Path, required: tuple[str, ...]) -> None:
    """Require durable documentation markers without duplicating prose."""
    text = path.read_text(encoding="utf-8")
    for marker in required:
        if marker not in text:
            failures.append(f"{path.relative_to(ROOT)} is missing required marker: {marker}")


def markdown_section(text: str, heading: str) -> str:
    """Return one level-two Markdown section for authority checks."""
    marker = f"## {heading}"
    if marker not in text:
        return ""
    section = text.split(marker, 1)[1]
    return section.split("\n## ", 1)[0]

link_pattern = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
for md_file in sorted([ROOT / "README.md", *DOCS.rglob("*.md")]):
    text = md_file.read_text(encoding="utf-8")
    for match in link_pattern.finditer(text):
        href = match.group(1).split("#", 1)[0]
        if not href or href.startswith(("http://", "https://", "mailto:")):
            continue
        if not href.endswith((".md", ".html")):
            continue
        target = (md_file.parent / href).resolve()
        try:
            target.relative_to(ROOT)
        except ValueError:
            failures.append(f"{md_file.relative_to(ROOT)} links outside repository: {href}")
            continue
        if not target.exists():
            failures.append(f"{md_file.relative_to(ROOT)} has missing link: {href}")

shell = DOCS / "assets" / "js" / "docs-shell.js"
shell_text = shell.read_text(encoding="utf-8")
for md_path in re.findall(r"md: '([^']+\.md)'", shell_text):
    if not (DOCS / md_path).exists():
        failures.append(f"docs-shell.js navigation points to missing markdown: {md_path}")
for load_path in re.findall(r"loadMarkdown\('([^']+\.md)'\)", "\n".join(p.read_text(encoding="utf-8") for p in DOCS.glob("*.html"))):
    if not (DOCS / load_path).exists():
        failures.append(f"HTML wrapper loads missing markdown: {load_path}")

# Keep the living code-health contract discoverable and prevent the dated
# review or a hand-maintained hotspot inventory from becoming authoritative.
require_text(ROOT / "AGENTS.md", ("docs/developer/code_health.md", "tests/source_file_size_policy.json"))
require_text(DOCS / "developer" / "code_health.md", (
    "tests/source_file_size_policy.json",
    "tests/check_repository_hygiene.py",
    "github_actions_workflows.md",
    "documentation_policy.md",
))
require_text(DOCS / "developer" / "code_health_review_2026-02-13.md", (
    "Historical evidence — not current policy",
    "[Code Health](code_health.md)",
))
if "following `docs/developer/code_health_review_2026-02-13.md`" in (ROOT / "AGENTS.md").read_text(encoding="utf-8"):
    failures.append("AGENTS.md presents the February 2026 review as current authority")

# Keep living owners separate from dated evidence in the canonical entry map.
developer_index = (DOCS / "developer" / "index.md").read_text(encoding="utf-8")
canonical_section = markdown_section(developer_index, "Canonical project-wide sources")
historical_section = markdown_section(developer_index, "Historical / validation evidence")
for canonical_link in (
    "[Architecture](architecture.md)",
    "[Repository Layout](repository_layout.md)",
    "[Code Health](code_health.md)",
    "[Build and Dependency Guide](build.md)",
    "[Packaging](packaging.md)",
    "[GitHub Actions workflow architecture](github_actions_workflows.md)",
    "[Localization](localization.md)",
):
    if canonical_link not in canonical_section:
        failures.append(f"developer index canonical section is missing {canonical_link}")
if "validation" in canonical_section.lower() or "audit.md" in canonical_section:
    failures.append("developer index presents audit or validation evidence as canonical")
if "repository_organization_regression_audit.md" not in historical_section:
    failures.append("developer index does not classify repository audit evidence as historical")
if "ci_release_packaging_validation.md" not in historical_section:
    failures.append("developer index does not classify CI/release validation as historical")

readme_text = (ROOT / "README.md").read_text(encoding="utf-8")
if re.search(r"docs/developer/[^)\s]*(?:audit|validation)[^)\s]*\.md", readme_text, re.IGNORECASE):
    failures.append("README links directly to historical audit or validation evidence")

if failures:
    print("Documentation link check failed:", file=sys.stderr)
    for failure in failures:
        print(f"- {failure}", file=sys.stderr)
    sys.exit(1)

print("OK: documentation links and website markdown targets exist.")
