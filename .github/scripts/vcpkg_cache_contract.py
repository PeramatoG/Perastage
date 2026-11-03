#!/usr/bin/env python3
"""Shared binary-only Actions cache contract; vcpkg remains the ABI authority."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import subprocess
from pathlib import Path

SCHEMA = "v5"
CACHE_PATH = ".vcpkg-cache/binary"


def digest(value: object) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def probe(*command: str) -> str:
    return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT).strip()


def environment_identity(runner_os: str, sdk: str) -> tuple[str, dict[str, str]]:
    tools = {"cmake": probe("cmake", "--version").splitlines()[0],
             "ninja": probe("ninja", "--version")}
    if runner_os == "Linux":
        release = platform.freedesktop_os_release()
        runtime = f"{release['ID']}-{release['VERSION_ID']}"
        tools.update(gcc=probe("gcc", "-dumpfullversion", "-dumpversion"),
                     gxx=probe("g++", "-dumpfullversion", "-dumpversion"),
                     target=probe("gcc", "-dumpmachine"),
                     libc=probe("getconf", "GNU_LIBC_VERSION"),
                     linker=probe("ld", "--version").splitlines()[0])
    elif runner_os == "macOS":
        if not sdk:
            raise ValueError("macOS requires the existing resolved SDK/Xcode identity")
        runtime = f"sdk-{sdk}"
        tools["clang"] = probe("clang", "--version").splitlines()[0]
    elif runner_os == "Windows":
        runtime = "windows"
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        vs = Path(probe(str(vswhere), "-latest", "-products", "*", "-requires",
                        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"))
        tools["msvc"] = (vs / "VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt").read_text().strip()
        sdk_root = Path(os.environ["ProgramFiles(x86)"]) / "Windows Kits/10/Include"
        tools["windows-sdk"] = max((p.name for p in sdk_root.iterdir()
                                   if p.is_dir() and p.name.startswith("10.")),
                                  key=lambda value: tuple(map(int, value.split("."))))
    else:
        raise ValueError(f"unsupported runner OS: {runner_os}")
    return runtime, tools


def keys(runner_os: str, arch: str, triplet: str, runtime: str,
         tools: dict[str, str], baseline: str, manifest: str) -> dict[str, str]:
    boundary = f"vcpkg-binary-{SCHEMA}-{runner_os}-{arch}-{triplet}-{runtime}-"
    tool_boundary = f"{boundary}{digest(tools)[:20]}-{baseline}-"
    return {"prefix": f"{tool_boundary}{manifest}-", "fallback": tool_boundary,
            "runtime-fallback": boundary, "path": CACHE_PATH, "schema": SCHEMA,
            "environment": json.dumps({"runtime": runtime, **tools}, sort_keys=True)}


def snapshot(root: Path) -> tuple[str, int]:
    """Content-address snapshots so immutable incomplete entries can be superseded."""
    inventory = []
    for archive in sorted(root.rglob("*.zip")):
        if archive.is_symlink():
            raise ValueError("binary cache must not contain symlink archives")
        checksum = hashlib.sha256()
        with archive.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                checksum.update(block)
        inventory.append((archive.relative_to(root).as_posix(), checksum.hexdigest()))
    return digest(inventory), len(inventory)


def snapshot_key(prefix: str, checksum: str, matched_key: str) -> str:
    base = prefix + checksum
    if matched_key == base or matched_key.startswith(base + "-repair-"):
        return matched_key
    # A repaired inventory may equal an older snapshot. Including its predecessor
    # permits a fresh save even when that older immutable key already exists.
    return base + ("-repair-" + digest(matched_key)[:16] if matched_key else "")


def emit(values: dict[str, str]) -> None:
    if "environment" in values and os.environ.get("GITHUB_ENV"):
        with Path(os.environ["GITHUB_ENV"]).open("a", encoding="utf-8") as output:
            output.write(f"VCPKG_CACHE_ENVIRONMENT={values['environment']}\n")
    for name, value in values.items():
        print(f"{name}={value}")
        if os.environ.get("GITHUB_OUTPUT"):
            with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
                output.write(f"{name}={value}\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    identity = commands.add_parser("identity")
    identity.add_argument("--triplet", required=True)
    identity.add_argument("--baseline", required=True)
    identity.add_argument("--manifest", required=True)
    identity.add_argument("--sdk", default="")
    save = commands.add_parser("snapshot")
    save.add_argument("--prefix", required=True)
    save.add_argument("--matched-key", default="")
    save.add_argument("--root", type=Path, default=Path(CACHE_PATH))
    args = parser.parse_args()
    if args.command == "identity":
        runner_os = os.environ["RUNNER_OS"]
        runtime, tools = environment_identity(runner_os, args.sdk)
        emit(keys(runner_os, os.environ["RUNNER_ARCH"], args.triplet, runtime,
                  tools, args.baseline, args.manifest))
    else:
        checksum, count = snapshot(args.root)
        emit({"key": snapshot_key(args.prefix, checksum, args.matched_key), "count": str(count)})


if __name__ == "__main__":
    main()
