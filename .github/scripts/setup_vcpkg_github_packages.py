#!/usr/bin/env python3
"""Configure vcpkg's optional GitHub Packages NuGet binary cache."""
from __future__ import annotations

import argparse
import os
import platform
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

FEED_URL = "https://nuget.pkg.github.com/PeramatoG/index.json"
SOURCE_NAME = "PerastageGitHubPackages"
REPOSITORY_URL = "https://github.com/PeramatoG/Perastage"


def redact_diagnostics(value: str, secrets: tuple[str, ...] = ()) -> str:
    """Remove known secrets and NuGet credential values from diagnostic text."""
    redacted = value or ""
    for secret in secrets:
        if secret:
            redacted = redacted.replace(secret, "[REDACTED]")
    redacted = re.sub(
        r"(<(?:add\s+key=[\"'](?:ClearTextPassword|Password|[^\"']*ApiKey)[\"']\s+value=[\"']))[^\"']*",
        r"\1[REDACTED]",
        redacted,
        flags=re.IGNORECASE,
    )
    redacted = re.sub(
        r"((?:-Password|setApiKey)\s+)(\S+)", r"\1[REDACTED]", redacted,
        flags=re.IGNORECASE,
    )
    return redacted


class SetupFailure(RuntimeError):
    """Describe a failed setup stage without exposing its credentials."""

    def __init__(self, stage: str, error: BaseException, secrets: tuple[str, ...] = ()) -> None:
        exit_code = getattr(error, "returncode", "unavailable")
        stdout = redact_diagnostics(getattr(error, "stdout", "") or "", secrets).strip()
        stderr = redact_diagnostics(getattr(error, "stderr", "") or str(error), secrets).strip()
        super().__init__(
            f"GitHub Packages setup stage '{stage}' failed (exit code: {exit_code}); "
            f"stdout: {stdout or '<empty>'}; stderr: {stderr or '<empty>'}"
        )


def run_stage(stage: str, command: list[str], secrets: tuple[str, ...]) -> subprocess.CompletedProcess:
    """Run one NuGet setup stage and convert process errors to safe diagnostics."""
    try:
        return subprocess.run(command, check=True, capture_output=True, text=True)
    except (OSError, subprocess.CalledProcessError) as error:
        raise SetupFailure(stage, error, secrets) from error


def append_command_file(variable: str, name: str, value: str) -> None:
    path = os.environ.get(variable)
    if path:
        with Path(path).open("a", encoding="utf-8") as stream:
            stream.write(f"{name}={value}\n")


def nuget_command(nuget: str) -> list[str]:
    return [nuget] if platform.system() == "Windows" else ["mono", nuget]


def find_section(root: ET.Element, name: str) -> ET.Element | None:
    """Find a direct NuGet configuration section without depending on XML namespaces."""
    return next((child for child in root if child.tag.rsplit("}", 1)[-1] == name), None)


def find_add(section: ET.Element | None, key: str) -> ET.Element | None:
    """Find an add entry by key without reading unrelated configuration values."""
    if section is None:
        return None
    return next((entry for entry in section
                 if entry.tag.rsplit("}", 1)[-1] == "add" and entry.get("key") == key), None)


def validate_config(config: Path, mode: str) -> None:
    """Validate the generated NuGet configuration structurally with safe diagnostics."""
    try:
        root = ET.parse(config).getroot()
    except (OSError, ET.ParseError) as error:
        raise RuntimeError("invalid NuGet configuration XML") from error

    source = find_add(find_section(root, "packageSources"), SOURCE_NAME)
    if source is None:
        raise RuntimeError("missing package source")
    if source.get("value") != FEED_URL:
        raise RuntimeError("unexpected feed URL")

    credentials = find_section(root, "packageSourceCredentials")
    credential_section = find_section(credentials, SOURCE_NAME) if credentials is not None else None
    if credential_section is None:
        raise RuntimeError("missing credential section")
    username = find_add(credential_section, "Username")
    if username is None or username.get("value") != "PeramatoG":
        raise RuntimeError("missing username")
    password = find_add(credential_section, "ClearTextPassword")
    if password is None:
        password = find_add(credential_section, "Password")
    if password is None:
        raise RuntimeError("missing password entry")

    if mode == "readwrite":
        push_source = find_add(find_section(root, "config"), "defaultPushSource")
        if push_source is None or push_source.get("value") != FEED_URL:
            raise RuntimeError("missing default push source")
        if find_add(find_section(root, "apikeys"), FEED_URL) is None:
            raise RuntimeError("missing API-key entry")


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

    secrets = (token,)
    try:
        fetch_result = run_stage("fetch-nuget", [args.vcpkg, "fetch", "nuget"], secrets)
        fetched_lines = fetch_result.stdout.strip().splitlines()
        if not fetched_lines:
            raise SetupFailure("fetch-nuget", RuntimeError("vcpkg returned no NuGet path"), secrets)
        fetched = fetched_lines[-1]
        config_dir = Path(os.environ.get("RUNNER_TEMP", tempfile.gettempdir())) / "perastage-vcpkg-nuget"
        config_dir.mkdir(parents=True, exist_ok=True)
        config = config_dir / "NuGet.Config"
        command = nuget_command(fetched)
        run_stage("add-source", command + ["sources", "Add", "-Name", SOURCE_NAME,
                  "-Source", FEED_URL, "-UserName", "PeramatoG", "-Password", token,
                  "-StorePasswordInClearText", "-ConfigFile", str(config),
                  "-NonInteractive"], secrets)
        if args.mode == "readwrite":
            run_stage("set-default-push-source", command + ["config", "-Set",
                      f"defaultPushSource={FEED_URL}", "-ConfigFile", str(config),
                      "-NonInteractive"], secrets)
            run_stage("set-api-key", command + ["setApiKey", token, "-Source", FEED_URL,
                      "-ConfigFile", str(config), "-NonInteractive"], secrets)
        try:
            validate_config(config, args.mode)
        except RuntimeError as error:
            raise SetupFailure("validate-config", error, secrets) from error
    except SetupFailure as error:
        if args.mode == "readwrite":
            raise RuntimeError(str(error)) from error
        print(f"::warning::{error}; using the local vcpkg cache only.")
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
