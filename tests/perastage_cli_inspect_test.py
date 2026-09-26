#!/usr/bin/env python3
"""Exercise the real CLI inspect command with deterministic synthetic packages."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

GDTF_XML = ('<GDTF DataVersion="1.2"><FixtureType Name="Fixture" Manufacturer="Perastage" '
            'Description="Valid fixture" FixtureTypeID="12345678-1234-4234-9234-123456789abc">'
            '<AttributeDefinitions><FeatureGroups/><Attributes/></AttributeDefinitions>'
            '<Geometries><Geometry Name="Root"/></Geometries><DMXModes>'
            '<DMXMode Name="Mode" Geometry="Root"><DMXChannels/></DMXMode>'
            '</DMXModes></FixtureType></GDTF>')
MVR_XML = ('<GeneralSceneDescription verMajor="1" verMinor="6" provider="Perastage" '
           'providerVersion="1.7"><UserData><Data provider="Example"/></UserData><Scene><Layers>'
           '<Layer uuid="10000000-0000-4000-8000-000000000001" name="Main">'
           '<ChildList/></Layer></Layers></Scene></GeneralSceneDescription>')


def write_package(path: Path, root_name: str, xml: str) -> None:
    """Write one small standards-oriented ZIP package without production writers."""
    with zipfile.ZipFile(path, "w", zipfile.ZIP_STORED) as package:
        package.writestr(root_name, xml)
        package.writestr("resources/readme.txt", "resource")


def run(executable: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    """Launch the real executable while preserving stdout and stderr separately."""
    return subprocess.run([str(executable), *arguments], text=True, capture_output=True, check=False)


def require(condition: bool, message: str) -> None:
    """Fail the integration test with one concise assertion message."""
    if not condition:
        raise AssertionError(message)


def main() -> int:
    """Verify summaries, views, JSON, failures, compatibility, and Unicode paths."""
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="perastage-cli-inspect-") as directory:
        root = Path(directory)
        gdtf = root / "fixture.gdtf"
        mvr = root / "scene.mvr"
        unicode_mvr = root / "escena-ñ.mvr"
        compatibility = root / "compat.gdtf"
        malformed = root / "broken.mvr"
        unsupported = root / "notes.txt"
        write_package(gdtf, "description.xml", GDTF_XML)
        write_package(mvr, "GeneralSceneDescription.xml", MVR_XML)
        write_package(unicode_mvr, "GeneralSceneDescription.xml", MVR_XML)
        write_package(compatibility, "Description.xml", GDTF_XML)
        malformed.write_bytes(b"not a zip")
        unsupported.write_text("text", encoding="utf-8")

        result = run(executable, "inspect", str(gdtf))
        require(result.returncode == 0 and "Format: GDTF" in result.stdout and not result.stderr,
                "valid GDTF summary")
        result = run(executable, "inspect", str(mvr))
        require(result.returncode == 0 and "Format: MVR" in result.stdout and not result.stderr,
                "valid MVR summary")
        result = run(executable, "inspect", str(mvr), "--view", "inventory")
        require(result.returncode == 0 and "GeneralSceneDescription.xml" in result.stdout,
                "inventory view")
        result = run(executable, "inspect", str(mvr), "--view", "resources")
        require(result.returncode == 0 and "GeneralSceneDescription.xml" in result.stdout
                and "raw-read=yes" in result.stdout, "resources view")
        result = run(executable, "inspect", str(mvr), "--view", "diagnostics")
        require(result.returncode == 0 and not result.stderr, "diagnostics view")
        result = run(executable, "inspect", str(mvr), "--view", "xml")
        require(result.returncode == 0 and result.stdout == MVR_XML, "exact XML output")
        result = run(executable, "inspect", str(gdtf), "--json")
        report = json.loads(result.stdout)
        require(result.returncode == 0 and report["format"] == "GDTF" and report["document"]["root_xml"] == GDTF_XML,
                "complete GDTF JSON")
        result = run(executable, "inspect", str(compatibility), "--json")
        require(result.returncode == 1 and json.loads(result.stdout)["status"] == "compatibility_accepted",
                "compatibility warning")
        result = run(executable, "inspect", str(malformed), "--json")
        require(result.returncode == 3 and json.loads(result.stdout)["success"] is False and not result.stderr,
                "structured malformed source")
        result = run(executable, "inspect", str(unsupported))
        require(result.returncode == 4 and not result.stdout and "unsupported input type" in result.stderr,
                "unsupported input routing")
        result = run(executable, "inspect", str(unicode_mvr), "--json")
        require(result.returncode == 0 and "escena-ñ.mvr" in json.loads(result.stdout)["request"]["source_path"],
                "Unicode filesystem path")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
