#!/usr/bin/env python3
"""Validate the actual embedded native manifests in the Windows release stage."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

NS = {"asm3": "urn:schemas-microsoft-com:asm.v3",
      "dpi2005": "http://schemas.microsoft.com/SMI/2005/WindowsSettings",
      "dpi2016": "http://schemas.microsoft.com/SMI/2016/WindowsSettings"}


def validate_manifest(content: bytes, *, gui: bool) -> None:
    tree = ET.fromstring(content)
    privilege = tree.find("asm3:trustInfo/asm3:security/asm3:requestedPrivileges/asm3:requestedExecutionLevel", NS)
    if privilege is None or privilege.get("level") != "asInvoker" or privilege.get("uiAccess") != "false":
        raise ValueError("Native Windows manifest must request asInvoker without UIAccess")
    legacy = tree.findall(".//dpi2005:dpiAware", NS)
    modern = tree.findall(".//dpi2016:dpiAwareness", NS)
    if gui:
        if (len(legacy) != 1 or (legacy[0].text or "").strip().lower() != "true" or
                len(modern) != 1 or (modern[0].text or "").strip().lower() != "system"):
            raise ValueError("GUI native manifest must declare system DPI awareness (not PerMonitorV2)")
    elif legacy or modern:
        raise ValueError("Console CLI manifest must not declare GUI DPI awareness")


def verify_executable(binary: Path, mt: Path, *, gui: bool) -> None:
    with tempfile.TemporaryDirectory(prefix="Perastage embedded manifest ") as temporary:
        output = Path(temporary) / "embedded.manifest"
        result = subprocess.run([str(mt), "-nologo", f"-inputresource:{binary};#1", f"-out:{output}"],
                                capture_output=True, text=True, check=False)
        if result.returncode != 0 or not output.is_file():
            raise ValueError(f"Cannot extract native manifest resource #1 from {binary}: "
                             f"{result.stdout}{result.stderr}")
        validate_manifest(output.read_bytes(), gui=gui)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gui", type=Path, required=True)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--mt", type=Path, required=True, help="Windows SDK mt.exe")
    arguments = parser.parse_args()
    verify_executable(arguments.gui, arguments.mt, gui=True)
    verify_executable(arguments.cli, arguments.mt, gui=False)
    print("OK: staged GUI system-DPI and console CLI manifests are embedded, asInvoker, and valid.")


if __name__ == "__main__":
    main()
