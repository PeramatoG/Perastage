#!/usr/bin/env python3
"""Validate local documentation links and website markdown targets."""
from __future__ import annotations

import pathlib
import re
import sys
from collections import Counter
from urllib.parse import unquote, urlsplit
from docs_static_validation import validate_static_docs

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

def manual_anchors(text: str) -> set[str]:
    """Match core/markdown.cpp: UTF-8 bytes retained, ASCII lowercase, duplicate suffixes."""
    anchors: set[str] = set()
    counts: Counter[str] = Counter()
    fenced = False
    text = re.sub(r"<!--.*?-->", "", text, flags=re.DOTALL)
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("```"):
            fenced = not fenced
        if fenced:
            continue
        heading = re.match(r"^#{1,3} (.*)$", line)
        if not heading:
            continue
        anchor = "".join(c.lower() if "A" <= c <= "Z" else
                         "-" if c in " \t" else c
                         for c in heading[1]
                         if ord(c) >= 128 or c.isascii() and
                         (c.isalnum() or c in "_- \t"))
        occurrence = counts[anchor]
        counts[anchor] += 1
        anchors.add(anchor + (f"-{occurrence}" if occurrence else ""))
    return anchors


link_pattern = re.compile(r"\[([^\]]*)\]\(([^)]*)\)")
for md_file in sorted([ROOT / "README.md", *DOCS.rglob("*.md")]):
    try:
        text = md_file.read_text(encoding="utf-8")
    except UnicodeError:
        failures.append(f"{md_file.relative_to(ROOT)} is not UTF-8")
        continue
    for match in link_pattern.finditer(text):
        label, destination = match.groups()
        if not label.strip() or not destination.strip():
            failures.append(f"{md_file.relative_to(ROOT)} has an empty Markdown link")
            continue
        href, _, fragment = destination.partition("#")
        if md_file.is_relative_to(DOCS / "user") and not urlsplit(destination).scheme:
            target = (md_file.parent / unquote(href)).resolve() if href else md_file
            if fragment and target.is_file() and target.suffix == ".md":
                try:
                    target_text = target.read_text(encoding="utf-8")
                    if unquote(fragment) not in manual_anchors(target_text):
                        failures.append(f"{md_file.relative_to(ROOT)} has missing fragment: {destination}")
                except UnicodeError:
                    failures.append(f"{target.relative_to(ROOT)} is not UTF-8")
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

# Validate manual ownership without duplicating the index or local-link checker.
manual = DOCS / "user"
if not (manual / "index.md").is_file():
    failures.append("canonical user manual index is missing")
canonical_names = {page.name for page in manual.glob("*.md") if page.is_file()}
locales = manual / "locales"
registry = (ROOT / "cmake" / "PerastageLocalization.cmake").read_text(encoding="utf-8")
locale_match = re.search(r"set\(PERASTAGE_TRANSLATION_LANGUAGES\s+([^)]+)\)", registry)
if not locale_match:
    failures.append("manual locales cannot resolve the existing translation registry")
allowed_locales = set(locale_match.group(1).split()) if locale_match else set()
for locale in sorted(allowed_locales):
    localized = locales / locale
    names = {page.name for page in localized.glob("*.md") if page.is_file()}
    for name in sorted(canonical_names - names):
        failures.append(f"{locale} manual is missing canonical page: {name}")
    for entry in localized.iterdir() if localized.is_dir() else []:
        if not entry.is_file() or entry.suffix != ".md":
            failures.append(f"localized manual has unexpected entry: {entry.relative_to(ROOT)}")
    for name in sorted(names - canonical_names):
        failures.append(f"{locale} manual has unexpected page: {name}")
for directory in sorted(locales.iterdir()) if locales.exists() else []:
    if not directory.is_dir() or directory.name not in allowed_locales:
        failures.append(f"unexpected localized manual directory: {directory.relative_to(ROOT)}")
for directory in [manual, *(locales / locale for locale in sorted(allowed_locales))]:
    for page in directory.glob("*.md"):
        try:
            if not page.read_text(encoding="utf-8").strip():
                failures.append(f"{page.relative_to(ROOT)} is empty")
        except UnicodeError:
            failures.append(f"{page.relative_to(ROOT)} is not UTF-8")

begin = "<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->"
end = "<!-- PERASTAGE_CONSOLE_HELP_END -->"
command_pages = [manual / "shortcuts-and-command-bar.md"]
command_pages.extend(locales.glob("*/shortcuts-and-command-bar.md"))
for page in command_pages:
    try:
        text = page.read_text(encoding="utf-8")
    except UnicodeError:
        continue
    if text.count(begin) != 1 or text.count(end) != 1 or text.find(begin) >= text.find(end):
        failures.append(f"{page.relative_to(ROOT)} must have exactly one ordered Console help marker pair")
    elif page.parent != manual:
        canonical_text = command_pages[0].read_text(encoding="utf-8")
        section = text.split(begin)[1].split(end)[0]
        canonical_section = canonical_text.split(begin)[1].split(end)[0]
        fence_pattern = r"```[^\n]*\n(.*?)```"
        inline_section = re.sub(fence_pattern, "", section, flags=re.DOTALL)
        inline_canonical = re.sub(fence_pattern, "", canonical_section, flags=re.DOTALL)
        for pattern, localized, english in (
                (fence_pattern, section, canonical_section),
                (r"`([^`]+)`", inline_section, inline_canonical)):
            if re.findall(pattern, localized, re.DOTALL) != re.findall(pattern, english, re.DOTALL):
                failures.append(f"{page.relative_to(ROOT)} changes stable Console syntax/examples")

# Original external destinations must remain unchanged in each mirror. New
# canonical developer/reference URLs are checked against their English owners.
for locale in sorted(allowed_locales):
    for source in manual.glob("*.md"):
        mirror = locales / locale / source.name
        if not mirror.is_file():
            continue
        try:
            translated_text = mirror.read_text(encoding="utf-8")
        except UnicodeError:
            continue
        original_urls = {m[2] for m in link_pattern.finditer(source.read_text(encoding="utf-8"))
                         if m[2].startswith(("http://", "https://"))}
        translated_urls = {m[2] for m in link_pattern.finditer(translated_text)
                           if m[2].startswith(("http://", "https://"))}
        if not original_urls <= translated_urls:
            failures.append(f"{mirror.relative_to(ROOT)} rewrites an external URL")
        prefix = "https://github.com/PeramatoG/Perastage/blob/main/docs/"
        for url in translated_urls:
            if url.startswith(prefix):
                target = (DOCS / url[len(prefix):].split("#")[0]).resolve()
                if not target.is_relative_to(DOCS) or not target.is_file():
                    failures.append(f"{mirror.relative_to(ROOT)} links to missing English owner: {url}")

# The canonical Markdown index owns grouped static navigation.
index_text = (manual / "index.md").read_text(encoding="utf-8")
canonical_navigation = []
for heading, body in re.findall(r"^## ([^\n]+)\n(.*?)(?=^## |\Z)", index_text, re.MULTILINE | re.DOTALL):
    entries = [(f"user/{path}", title) for title, path in link_pattern.findall(body)
               if path in canonical_names and path != "index.md"]
    if entries:
        canonical_navigation.append((heading, entries))
indexed_paths = [path for _, entries in canonical_navigation for path, _ in entries]
expected_paths = {f"user/{name}" for name in canonical_names - {"index.md"}}
if set(indexed_paths) != expected_paths or len(indexed_paths) != len(set(indexed_paths)):
    failures.append("canonical user index must list every user page exactly once")

validate_static_docs(ROOT, canonical_navigation, failures)

# Keep the living code-health contract and machine-owned inventories discoverable.
require_text(ROOT / "AGENTS.md", ("docs/developer/code_health.md", "tests/source_file_size_policy.json"))
require_text(DOCS / "developer" / "code_health.md", (
    "tests/source_file_size_policy.json",
    "tests/check_repository_hygiene.py",
    "github_actions_workflows.md",
    "documentation_policy.md",
))
# Keep the major living owners discoverable in the canonical entry map.
developer_index = (DOCS / "developer" / "index.md").read_text(encoding="utf-8")
canonical_section = markdown_section(developer_index, "Canonical project-wide sources")
for canonical_link in (
    "[Architecture](architecture.md)",
    "[Repository Layout](repository_layout.md)",
    "[Code Health](code_health.md)",
    "[Maintainer Runbook](maintainer_runbook.md)",
    "[Build and Dependency Guide](build.md)",
    "[Packaging](packaging.md)",
    "[GitHub Actions workflow architecture](github_actions_workflows.md)",
    "[Versioning Policy](versioning-policy.md)",
    "[Localization](localization.md)",
):
    if canonical_link not in canonical_section:
        failures.append(f"developer index canonical section is missing {canonical_link}")
if "validation" in canonical_section.lower() or "audit.md" in canonical_section:
    failures.append("developer index presents audit or validation evidence as canonical")

# Keep the operational index connected to the detailed procedure owners.
require_text(DOCS / "developer" / "maintainer_runbook.md", (
    "[Build](build.md)",
    "[Packaging](packaging.md)",
    "[GitHub Actions workflow architecture](github_actions_workflows.md)",
    "[Main branch protection](main_branch_protection.md)",
))

readme_text = (ROOT / "README.md").read_text(encoding="utf-8")
if re.search(r"docs/developer/[^)\s]*(?:audit|validation)[^)\s]*\.md", readme_text, re.IGNORECASE):
    failures.append("README links directly to historical audit or validation evidence")

if failures:
    print("Documentation link check failed:", file=sys.stderr)
    for failure in failures:
        print(f"- {failure}", file=sys.stderr)
    sys.exit(1)

print(f"OK: documentation links, localized fragments, UTF-8, Console syntax and manual parity ({len(canonical_names)} pages per locale).")
