#!/usr/bin/env python3
"""Configure vcpkg's optional GitHub Packages NuGet binary cache."""
from __future__ import annotations

import argparse
import os
import platform
import subprocess
import sys
import tempfile
from pathlib import Path

FEED_URL = "https://nuget.pkg.github.com/PeramatoG/index.json"
SOURCE_NAME = "PerastageGitHubPackages"
REPOSITORY_URL = "https://github.com/PeramatoG/Perastage"


def append_command_file(variable: str, name: str, value: str) -> None:
    path = os.environ.get(variable)
    if path:
        with Path(path).open("a", encoding="utf-8") as stream:
            stream.write(f"{name}={value}\n")


def nuget_command(nuget: str) -> list[str]:
    return [nuget] if platform.system() == "Windows" else ["mono", nuget]


def configure(args: argparse.Namespace) -> bool:
    local_sources = f"clear;files,{Path(args.local_cache).resolve()},readwrite"
    append_command_file("GITHUB_ENV", "VCPKG_NUGET_REPOSITORY", REPOSITORY_URL)
    if args.mode == "disabled":
        append_command_file("GITHUB_ENV", "VCPKG_BINARY_SOURCES", local_sources)
        return False

    token = os.environ.get("GITHUB_TOKEN", "")
    if not token:
        if args.mode == "readwrite":
            raise RuntimeError("GITHUB_TOKEN is required for trusted cache publication")
        print("::warning::GitHub Packages token is unavailable; using the local vcpkg cache only.")
        append_command_file("GITHUB_ENV", "VCPKG_BINARY_SOURCES", local_sources)
        return False

    try:
        fetched = subprocess.run(
            [args.vcpkg, "fetch", "nuget"], check=True, capture_output=True, text=True
        ).stdout.strip().splitlines()[-1]
        config_dir = Path(os.environ.get("RUNNER_TEMP", tempfile.gettempdir())) / "perastage-vcpkg-nuget"
        config_dir.mkdir(parents=True, exist_ok=True)
        config = config_dir / "NuGet.Config"
        command = nuget_command(fetched)
        subprocess.run(command + ["sources", "Add", "-Name", SOURCE_NAME, "-Source", FEED_URL,
                       "-UserName", "PeramatoG", "-Password", token, "-StorePasswordInClearText",
                       "-ConfigFile", str(config), "-NonInteractive"], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        if args.mode == "readwrite":
            subprocess.run(command + ["config", "-Set", f"defaultPushSource={FEED_URL}",
                           "-ConfigFile", str(config), "-NonInteractive"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            subprocess.run(command + ["setApiKey", token, "-Source", FEED_URL,
                           "-ConfigFile", str(config), "-NonInteractive"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        subprocess.run(command + ["list", "-Source", SOURCE_NAME, "-ConfigFile", str(config),
                       "-NonInteractive"], check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.PIPE, text=True)
    except (OSError, subprocess.CalledProcessError) as error:
        if args.mode == "readwrite":
            raise RuntimeError("GitHub Packages setup failed for trusted publication") from error
        print("::warning::GitHub Packages setup failed; using the local vcpkg cache only.")
        append_command_file("GITHUB_ENV", "VCPKG_BINARY_SOURCES", local_sources)
        return False

    sources = f"{local_sources};nugetconfig,{config},{args.mode}"
    append_command_file("GITHUB_ENV", "VCPKG_BINARY_SOURCES", sources)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vcpkg", required=True)
    parser.add_argument("--local-cache", required=True)
    parser.add_argument("--mode", choices=("disabled", "read", "readwrite"), required=True)
    args = parser.parse_args()
    try:
        enabled = configure(args)
    except (OSError, subprocess.CalledProcessError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    append_command_file("GITHUB_OUTPUT", "remote-enabled", "true" if enabled else "false")
    append_command_file("GITHUB_OUTPUT", "setup-result", "configured" if enabled else "local-only")
    append_command_file("GITHUB_OUTPUT", "fallback-local-only", "false" if enabled else "true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
