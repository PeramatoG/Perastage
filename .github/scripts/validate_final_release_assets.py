#!/usr/bin/env python3
"""Validate final Perastage release assets and write checksums/provenance."""
from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

LEGACY_SHA = "c857665b99aacf9f466edd4416584dfb56ac1a1f"
LEGACY_VERSION = "1.5.0"
INSTALLER_EXTENSIONS = (".exe", ".AppImage", ".dmg", ".pkg.tar.zst", ".zip")


def load_contract(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def ensure_safe_tree(root: Path) -> None:
    resolved_root = root.resolve()
    for entry in root.rglob("*"):
        if entry.is_symlink():
            target = entry.resolve(strict=False)
            try:
                target.relative_to(resolved_root)
            except ValueError as exc:
                raise SystemExit(f"Escaping symlink is not allowed: {entry} -> {target}") from exc


def expected_patterns(contract: dict, version: str) -> dict[str, str]:
    patterns = dict(contract["packages"])
    patterns["debugSymbols"] = f"Perastage-{version}-Debug-Symbols-Developers-Only.zip"
    return patterns


def collect_assets(root: Path, contract: dict, version: str) -> list[Path]:
    ensure_safe_tree(root)
    files = sorted(path for path in root.iterdir() if path.is_file())
    patterns = expected_patterns(contract, version)
    matched: dict[str, list[Path]] = {name: [] for name in patterns}
    for path in files:
        for name, pattern in patterns.items():
            if fnmatch.fnmatch(path.name, pattern):
                matched[name].append(path)
    errors: list[str] = []
    for name, paths in matched.items():
        if len(paths) != 1:
            errors.append(f"{name} count {len(paths)} for pattern {patterns[name]}")
    allowed = {paths[0] for paths in matched.values() if len(paths) == 1}
    for path in files:
        if path not in allowed and path.name.endswith(INSTALLER_EXTENSIONS):
            errors.append(f"Unexpected release asset: {path.name}")
    version_token = version.replace(".", r"[._-]")
    version_re = re.compile(rf"(^|[^0-9]){version_token}([^0-9]|$)")
    for path in allowed:
        if not version_re.search(path.name):
            errors.append(f"Stale asset version in filename: {path.name}")
        if path.stat().st_size <= 0:
            errors.append(f"Empty release asset: {path.name}")
    if errors:
        raise SystemExit("\n".join(errors))
    return sorted(allowed)


def write_checksums(assets: list[Path], output: Path | None) -> dict[str, str]:
    checksums = {asset.name: sha256_file(asset) for asset in assets}
    if output:
        output.write_text("".join(f"{digest}  {name}\n" for name, digest in checksums.items()), encoding="utf-8")
    return checksums


def write_provenance(args: argparse.Namespace, assets: list[Path], checksums: dict[str, str]) -> None:
    if not args.provenance_output:
        return
    payload = {
        "repository": args.repository,
        "workflow_run_id": args.workflow_run_id,
        "workflow_run_attempt": args.workflow_run_attempt,
        "base_sha": args.base_sha,
        "release_sha": args.release_sha,
        "release_version": args.version,
        "tag": f"v{args.version}",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "final_asset_filenames": [asset.name for asset in assets],
        "sha256": checksums,
    }
    args.provenance_output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def validate_provenance(root: Path, release_sha: str, version: str) -> None:
    path = root / "release-provenance.json"
    if not path.exists():
        if release_sha == LEGACY_SHA and version == LEGACY_VERSION:
            return
        raise SystemExit("Missing release-provenance.json in validated artifact")
    data = json.loads(path.read_text(encoding="utf-8"))
    expected = {"release_sha": release_sha, "release_version": version, "tag": f"v{version}"}
    for key, value in expected.items():
        if data.get(key) != value:
            raise SystemExit(f"Provenance {key} mismatch: expected {value}, got {data.get(key)}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets-dir", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--contract", default=Path(".github/release-artifact-contract.json"), type=Path)
    parser.add_argument("--checksums-output", type=Path)
    parser.add_argument("--provenance-output", type=Path)
    parser.add_argument("--validate-provenance", action="store_true")
    parser.add_argument("--release-sha", default="")
    parser.add_argument("--repository", default=os.environ.get("GITHUB_REPOSITORY", ""))
    parser.add_argument("--workflow-run-id", default=os.environ.get("GITHUB_RUN_ID", ""))
    parser.add_argument("--workflow-run-attempt", default=os.environ.get("GITHUB_RUN_ATTEMPT", ""))
    parser.add_argument("--base-sha", default="")
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.version):
        raise SystemExit(f"Invalid semantic version: {args.version}")
    assets = collect_assets(args.assets_dir, load_contract(args.contract), args.version)
    if args.validate_provenance:
        validate_provenance(args.assets_dir, args.release_sha, args.version)
    checksums = write_checksums(assets, args.checksums_output)
    write_provenance(args, assets, checksums)
    for name, digest in checksums.items():
        print(f"{digest}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
