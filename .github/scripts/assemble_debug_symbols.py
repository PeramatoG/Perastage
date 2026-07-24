#!/usr/bin/env python3
"""Assemble the developer-only release debug-symbol archive from workflow artifacts."""
from __future__ import annotations

import argparse
import shutil
import stat
import sys
import zipfile
from pathlib import Path

PLATFORMS = {
    "Windows-x64": {"artifact": "Perastage-windows-symbols", "pattern": "*.pdb", "kind": "files"},
    "Linux-AppImage-x86_64": {"artifact": "Perastage-linux-symbols", "pattern": "Perastage.debug", "kind": "zip_or_files"},
    "ArchLinux-x86_64": {"artifact": "Perastage-archlinux-symbols", "pattern": "perastage-debug-*.pkg.tar.zst", "kind": "zip_or_files"},
    "macOS15-arm64": {"artifact": "Perastage-macos15-symbols", "pattern": "Perastage.dSYM", "kind": "bundle"},
    "macOS26-arm64": {"artifact": "Perastage-macos26-symbols", "pattern": "Perastage.dSYM", "kind": "bundle"},
}


def find_artifact(root: Path, artifact: str) -> Path:
    matches = [p for p in root.rglob(artifact) if p.is_dir()]
    if len(matches) != 1:
        raise RuntimeError(f"Expected exactly one artifact directory named {artifact}, found {len(matches)}")
    return matches[0]


def find_required(base: Path, pattern: str, platform: str, allow_many: bool = False) -> list[Path]:
    matches = sorted(p for p in base.rglob(pattern) if p.is_file() or p.is_dir())
    if not matches:
        raise RuntimeError(f"Missing required {platform} symbol matching {pattern} in {base}")
    if not allow_many and len(matches) != 1:
        raise RuntimeError(f"Expected exactly one {platform} symbol matching {pattern}, found {len(matches)}")
    return matches


def extract_nested_zips(artifact_dir: Path, work_dir: Path) -> list[Path]:
    extracted: list[Path] = []
    for zip_path in sorted(artifact_dir.rglob("*.zip")):
        target = work_dir / zip_path.stem
        target.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(zip_path) as archive:
            archive.extractall(target)
        extracted.append(target)
    return extracted


def copy_path(source: Path, destination_dir: Path) -> None:
    target = destination_dir / source.name
    if source.is_dir():
        shutil.copytree(source, target, symlinks=True)
    else:
        shutil.copy2(source, target, follow_symlinks=False)


def write_readme(staging: Path, version: str) -> None:
    (staging / "README.txt").write_text(
        f"Perastage {version} developer debug symbols\n\n"
        "This archive is for maintainers and crash analysis only. Users do not need it to install or run Perastage.\n\n"
        "Match symbols by the exact Perastage version, platform, architecture, and release build variant. "
        "The macOS 15 and macOS 26 dSYM bundles come from separate builds and are not interchangeable.\n",
        encoding="utf-8",
    )


def add_to_zip(archive: zipfile.ZipFile, path: Path, arcname: Path) -> None:
    if path.is_symlink():
        info = zipfile.ZipInfo(str(arcname))
        info.create_system = 3
        info.external_attr = (stat.S_IFLNK | 0o777) << 16
        archive.writestr(info, path.readlink().as_posix())
    elif path.is_dir():
        for child in sorted(path.iterdir()):
            add_to_zip(archive, child, arcname / child.name)
    else:
        archive.write(path, arcname)


def create_zip(staging: Path, output: Path) -> None:
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for child in sorted(staging.iterdir()):
            add_to_zip(archive, child, Path(child.name))


def assemble(root: Path, version: str, output_dir: Path) -> Path:
    work = root / "symbols-work"
    staging = root / "symbols-final"
    shutil.rmtree(work, ignore_errors=True)
    shutil.rmtree(staging, ignore_errors=True)
    work.mkdir(parents=True)
    staging.mkdir(parents=True)

    for platform, spec in PLATFORMS.items():
        artifact_dir = find_artifact(root, spec["artifact"])
        search_roots = [artifact_dir] + extract_nested_zips(artifact_dir, work / platform)
        platform_dir = staging / platform
        platform_dir.mkdir()
        allow_many = platform in {"Windows-x64", "ArchLinux-x86_64"}
        matches: list[Path] = []
        for search_root in search_roots:
            matches.extend(sorted(p for p in search_root.rglob(spec["pattern"]) if p.is_file() or p.is_dir()))
        unique = sorted({p.resolve() for p in matches})
        if not unique:
            raise RuntimeError(f"Missing required {platform} symbols")
        if not allow_many and len(unique) != 1:
            raise RuntimeError(f"Expected exactly one {platform} symbol, found {len(unique)}")
        for source in unique:
            copy_path(source, platform_dir)

    write_readme(staging, version)
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / f"Perastage-{version}-Debug-Symbols-Developers-Only.zip"
    if output.exists():
        output.unlink()
    create_zip(staging, output)
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = assemble(args.artifacts, args.version, args.output_dir)
    except Exception as exc:
        print(f"Failed to assemble debug symbols: {exc}", file=sys.stderr)
        return 1
    print(f"Created {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
