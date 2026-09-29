#!/usr/bin/env python3
"""Pure dependency manifest, registry, comparison, and report operations."""

from __future__ import annotations

import json
from pathlib import Path


def load_json(path: Path) -> dict:
    """Load one UTF-8 JSON object."""
    return json.loads(path.read_text(encoding="utf-8"))


def direct_dependencies(manifest: dict) -> list[dict]:
    """Normalize and sort direct manifest dependency declarations."""
    result = []
    for entry in manifest["dependencies"]:
        item = {"name": entry} if isinstance(entry, str) else dict(entry)
        result.append({"name": item["name"], "role": "host" if item.get("host") else "runtime",
                       "features": sorted(item.get("features", []))})
    return sorted(result, key=lambda item: item["name"])


def resolve(manifest: dict, policy: dict, registry: Path) -> dict:
    """Resolve direct dependencies from a checkout fixed at the manifest baseline."""
    baseline = manifest["builtin-baseline"]
    default = load_json(registry / "versions" / "baseline.json")["default"]
    policies = {item["name"]: item for item in policy["vcpkg"]}
    packages = []
    for item in direct_dependencies(manifest):
        name = item["name"]
        port = load_json(registry / "ports" / name / "vcpkg.json")
        version = default[name]
        packages.append({**item, "version": version["baseline"],
                         "port-version": version.get("port-version", 0),
                         "maintenance-group": policies[name]["maintenance-group"],
                         "license": port.get("license")})
    return {"schema-version": 1, "generated": True, "baseline": baseline, "packages": packages}


def version_label(item: dict) -> str:
    """Format a vcpkg version with a nonzero port revision."""
    suffix = f"#{item['port-version']}" if item.get("port-version", 0) else ""
    return f"{item['version']}{suffix}"


def compare(old: dict, candidate: dict, policy: dict) -> list[dict]:
    """Return deterministically ordered direct changes with policy classifications."""
    old_by_name = {item["name"]: item for item in old["packages"]}
    policies = {item["name"]: item for item in policy["vcpkg"]}
    changes = []
    for new in candidate["packages"]:
        previous = old_by_name[new["name"]]
        if version_label(previous) == version_label(new) and previous.get("license") == new.get("license"):
            continue
        rule = policies[new["name"]]
        trigger = bool(rule["quarterly-candidate"] and version_label(previous) != version_label(new))
        old_major = previous["version"].split(".", 1)[0]
        new_major = new["version"].split(".", 1)[0]
        large_jump = old_major.isdigit() and new_major.isdigit() and old_major != new_major
        changes.append({"name": new["name"], "group": rule["maintenance-group"],
                        "old": version_label(previous), "candidate": version_label(new),
                        "reason": "trigger" if trigger else "collateral",
                        "license-changed": previous.get("license") != new.get("license"),
                        "large-version-jump": large_jump})
    return changes


def markdown_report(old: dict, candidate: dict, policy: dict, feature_ok: bool,
                    bundled_notice: str = "Notify-only checks were not requested.") -> str:
    """Render the candidate review report used in dry runs and draft PRs."""
    rows = compare(old, candidate, policy)
    lines = ["# Dependency review", "", f"Old baseline: `{old['baseline']}`  ",
             f"Candidate baseline: `{candidate['baseline']}`", "",
             "| Dependency | Maintenance group | Old | Candidate | Reason | License expression changed |",
             "| --- | --- | --- | --- | --- | --- |"]
    lines += [f"| {r['name']} | {r['group']} | {r['old']} | {r['candidate']} | {r['reason']} | {'yes' if r['license-changed'] else 'no'} |" for r in rows]
    flagged = [row["name"] for row in rows if row["large-version-jump"]]
    lines += ["", "Large version jumps: " + (", ".join(flagged) if flagged else "none detected") + "."]
    lines += ["", f"Required wxWidgets feature validation: **{'passed' if feature_ok else 'failed'}**.",
              f"Vendored dependency notifications: {bundled_notice}", "",
              "This candidate must not be auto-merged. Full CI must pass and a human must review all direct collateral and licensing changes."]
    return "\n".join(lines) + "\n"
