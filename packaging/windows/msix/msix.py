#!/usr/bin/env python3
"""Prepare and verify Store packaging inputs; MakeAppx owns schema validation."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import sys
from xml.sax.saxutils import escape
import xml.etree.ElementTree as ET
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
NS = {"f": "http://schemas.microsoft.com/appx/manifest/foundation/windows10"}
IDENTITY_KEYS = ("Name", "Publisher", "PublisherDisplayName")
ASSETS = {"StoreLogo.png": 50, "Square44x44Logo.png": 44,
          "Square150x150Logo.png": 150, "ProjectFile.png": 32, "MvrFile.png": 32}
PRIVATE_SUFFIXES = {".pdb", ".ilk", ".lib", ".exp", ".obj", ".pfx", ".p12",
                    ".pem", ".key", ".cer", ".msix", ".msixupload", ".appx"}
RESERVED = {"appxmanifest.xml", "appxblockmap.xml", "appxsignature.p7x",
            "[content_types].xml", "storeassets", "appxmetadata"}
FIXED_TIME = 946684800  # 2000-01-01 UTC; no build-clock version or input timestamps.
VCLIBS_NAME = "Microsoft.VCLibs.140.00.UWPDesktop"
VCLIBS_PUBLISHER = "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US"


def package_version(version: str) -> str:
    """Map canonical release major.minor.patch to major.minor.patch.0."""
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", version):
        raise ValueError("VERSION must be major.minor.patch without prefixes, suffixes or leading zeros")
    if any(len(part) > 5 for part in version.split(".")):
        raise ValueError("MSIX version components must be <= 65535")
    major, minor, patch = map(int, version.split("."))
    if not 1 <= major <= 65535 or max(minor, patch) > 65535:
        raise ValueError("Store MSIX version requires major 1..65535 and minor/patch 0..65535; revision is 0")
    return f"{major}.{minor}.{patch}.0"


def artifact_name(version: str) -> str:
    package_version(version)
    return f"Perastage_{version}_x64.msix"


def read_identity(path: Path | None) -> dict[str, str]:
    values = json.loads((path or HERE / "identity.json").read_text(encoding="utf-8"))
    if not isinstance(values, dict):
        raise ValueError("Partner Center identity must be a JSON object")
    for key in IDENTITY_KEYS:
        value = values.get(key)
        if (not isinstance(value, str) or not value.strip() or "<" in value or ">" in value
                or any(ord(char) < 32 for char in value)):
            raise ValueError(f"Missing/placeholder/invalid Partner Center {key}; supply its exact case-sensitive value")
        # Fail instead of silently trimming XML-normalized whitespace.
        if value != value.strip():
            raise ValueError(f"Partner Center {key} contains surrounding whitespace; verify the exact value")
    return {key: values[key] for key in IDENTITY_KEYS}


def framework_version(package: Path) -> str:
    """Read the retail x64 Desktop framework identity instead of guessing a CRT floor."""
    with zipfile.ZipFile(package) as archive:
        tree = ET.fromstring(archive.read("AppxManifest.xml"))
    entry = tree.find("f:Identity", NS)
    if (entry is None or entry.get("Name") != VCLIBS_NAME or
            entry.get("Publisher") != VCLIBS_PUBLISHER or entry.get("ProcessorArchitecture") != "x64" or
            tree.findtext("f:Properties/f:Framework", namespaces=NS) != "true"):
        raise ValueError("Expected Microsoft retail x64 Desktop VCLibs 14 framework package (not Debug/UWP-only)")
    version = entry.get("Version", "")
    if not re.fullmatch(r"[0-9]{1,5}(?:\.[0-9]{1,5}){3}", version) or any(int(part) > 65535 for part in version.split(".")):
        raise ValueError("Invalid VCLibs framework version")
    return version


def manifest(version: str, identity: dict[str, str], vclibs_version: str) -> bytes:
    replacements = {"IDENTITY_NAME": identity["Name"], "IDENTITY_PUBLISHER": identity["Publisher"],
                    "PUBLISHER_DISPLAY_NAME": identity["PublisherDisplayName"],
                    "PACKAGE_VERSION": package_version(version), "VCLIBS_VERSION": vclibs_version}
    source = (HERE / "AppxManifest.xml.in").read_text(encoding="utf-8")
    rendered = re.sub(r"@([A-Z_]+)@", lambda match: escape(replacements[match[1]], {'"': "&quot;"}), source)
    result = rendered.encode("utf-8")
    tree = ET.fromstring(result)
    entry = tree.find("f:Identity", NS)
    if entry is None or entry.get("Name") != identity["Name"] or entry.get("Publisher") != identity["Publisher"]:
        raise ValueError("Generated manifest identity differs from Partner Center input")
    if tree.findtext("f:Properties/f:PublisherDisplayName", namespaces=NS) != identity["PublisherDisplayName"]:
        raise ValueError("Generated PublisherDisplayName differs from Partner Center input")
    return result


def verify_pe(path: Path) -> None:
    """Prevent accidental packaging of an x86/ARM/debug fixture as the x64 release."""
    with path.open("rb") as stream:
        header = stream.read(64)
        if len(header) != 64 or header[:2] != b"MZ":
            raise ValueError(f"Not a Windows PE executable: {path}")
        stream.seek(struct.unpack_from("<I", header, 60)[0])
        pe = stream.read(6)
    if pe != b"PE\x00\x00\x64\x86":
        raise ValueError(f"Expected an x64 PE executable: {path}")


def staged_files(stage: Path) -> list[Path]:
    if not stage.is_dir():
        raise ValueError(f"Missing validated Release stage: {stage}")
    for name in ("Perastage.exe", "perastage-cli.exe", "LICENSE.txt", "THIRD_PARTY_NOTICES.md"):
        if not (stage / name).is_file():
            raise ValueError(f"Missing required staged file: {name}")
    for name in ("Perastage.exe", "perastage-cli.exe"):
        verify_pe(stage / name)
    for name in ("resources", "library"):
        if not (stage / name).is_dir():
            raise ValueError(f"Missing required staged directory: {name}")
    files = []
    seen = set()
    for path in sorted(stage.rglob("*")):
        relative = path.relative_to(stage)
        if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
            raise ValueError(f"Links/reparse points are not allowed in Store inputs: {relative}")
        if (path.suffix.lower() in PRIVATE_SUFFIXES or
                any(part.lower() in {"symbols", ".git", "__pycache__"} for part in relative.parts)
                or relative.parts[0].lower() in RESERVED):
            raise ValueError(f"Private, generated or reserved file in staged payload: {relative}")
        if path.is_file():
            lowered = relative.as_posix().lower()
            if lowered in seen:
                raise ValueError(f"Case-colliding package path: {relative}")
            seen.add(lowered)
            files.append(path)
    return files


def verify_assets(directory: Path) -> None:
    for name, size in ASSETS.items():
        path = directory / name
        if not path.is_file():
            raise ValueError(f"Missing MSIX visual asset: {path}")
        header = path.read_bytes()[:24]
        if (len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or
                header[12:16] != b"IHDR" or struct.unpack(">II", header[16:24]) != (size, size)):
            raise ValueError(f"Expected {size}x{size} PNG MSIX asset: {path}")


def prepare(stage: Path, layout: Path, version: str, identity: dict[str, str],
            vclibs_version: str,
            assets: Path = HERE / "assets") -> None:
    xml = manifest(version, identity, vclibs_version)
    files = staged_files(stage)
    verify_assets(assets)
    if layout.exists():
        raise ValueError(f"MSIX layout must be new (refusing stale/overlapping content): {layout}")
    if layout.resolve().is_relative_to(stage.resolve()):
        raise ValueError("MSIX layout must be outside the validated stage")
    layout.mkdir(parents=True)
    for path in files:
        target = layout / path.relative_to(stage)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
    (layout / "StoreAssets").mkdir()
    for name in ASSETS:
        shutil.copyfile(assets / name, layout / "StoreAssets" / name)
    (layout / "AppxManifest.xml").write_bytes(xml)
    for path in layout.rglob("*"):
        os.utime(path, (FIXED_TIME, FIXED_TIME))


def verify_archive(package: Path, layout: Path, version: str) -> None:
    """Require byte-exact staged payload, manifest and artwork in unsigned Store output."""
    if package.name != artifact_name(version):
        raise ValueError(f"Unexpected MSIX output name: {package.name}")
    expected = {path.relative_to(layout).as_posix(): path for path in layout.rglob("*") if path.is_file()}
    with zipfile.ZipFile(package) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError("Duplicate MSIX archive entries")
        extra = set(names) - set(expected)
        if extra - {"AppxBlockMap.xml", "[Content_Types].xml"}:
            raise ValueError(f"Unexpected MSIX content (including signatures/symbols): {sorted(extra)}")
        if set(expected) - set(names) or not {"AppxBlockMap.xml", "[Content_Types].xml"} <= set(names):
            raise ValueError("MSIX content is incomplete")
        for name, path in expected.items():
            digest = hashlib.sha256()
            with archive.open(name) as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(chunk)
            original = hashlib.sha256()
            with path.open("rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    original.update(chunk)
            if digest.digest() != original.digest():
                raise ValueError(f"MSIX altered staged content: {name}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("prepare", "verify", "name"))
    parser.add_argument("--stage", type=Path, default=ROOT / "out/install/Release")
    parser.add_argument("--layout", type=Path)
    parser.add_argument("--identity", type=Path)
    parser.add_argument("--package", type=Path)
    parser.add_argument("--vclibs-package", type=Path)
    args = parser.parse_args()
    try:
        version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
        package_version(version)
        if args.command == "name":
            print(artifact_name(version))
        else:
            if args.layout is None:
                raise ValueError("--layout is required")
            if args.command == "prepare":
                identity = read_identity(args.identity)
                if args.vclibs_package is None:
                    raise ValueError("--vclibs-package requires the retail x64 Desktop VCLibs framework matching the release toolchain")
                prepare(args.stage, args.layout, version, identity, framework_version(args.vclibs_package))
            elif args.package is None:
                raise ValueError("--package is required")
            else:
                verify_archive(args.package, args.layout, version)
    except (ValueError, OSError, KeyError, ET.ParseError, zipfile.BadZipFile) as error:
        print(f"STORE-100: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
