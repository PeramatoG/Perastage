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


def run(executable: Path, *arguments: str) -> tuple[int, str, str]:
    """Launch the real executable and decode its technical output as UTF-8."""
    result = subprocess.run([str(executable), *arguments], capture_output=True, check=False)
    return (result.returncode,
            result.stdout.decode("utf-8", errors="strict"),
            result.stderr.decode("utf-8", errors="strict"))


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
        unicode_name = "資料-灯具-ñ.mvr"
        unicode_mvr = root / unicode_name
        unsafe_mvr = root / "unsafe-entry.mvr"
        compatibility = root / "compat.gdtf"
        malformed = root / "broken.mvr"
        unsupported = root / "notes.txt"
        write_package(gdtf, "description.xml", GDTF_XML)
        write_package(mvr, "GeneralSceneDescription.xml", MVR_XML)
        write_package(unicode_mvr, "GeneralSceneDescription.xml", MVR_XML)
        write_package(unsafe_mvr, "GeneralSceneDescription.xml", MVR_XML)
        with zipfile.ZipFile(unsafe_mvr, "a", zipfile.ZIP_STORED) as package:
            package.writestr("../unsafe-resource.txt", "unsafe")
        write_package(compatibility, "Description.xml", GDTF_XML)
        malformed.write_bytes(b"not a zip")
        unsupported.write_text("text", encoding="utf-8")

        result = run(executable, "inspect", str(gdtf))
        require(result[0] == 0 and "Format: GDTF" in result[1] and not result[2],
                "valid GDTF summary")
        result = run(executable, "inspect", str(mvr))
        require(result[0] == 0 and "Format: MVR" in result[1] and not result[2],
                "valid MVR summary")
        result = run(executable, "inspect", str(mvr), "--view", "inventory")
        require(result[0] == 0 and "GeneralSceneDescription.xml" in result[1],
                "inventory view")
        result = run(executable, "inspect", str(mvr), "--view", "resources")
        require(result[0] == 0 and "GeneralSceneDescription.xml" in result[1]
                and "raw-read=yes" in result[1], "resources view")
        result = run(executable, "inspect", str(mvr), "--view", "diagnostics")
        require(result[0] == 0 and not result[2], "diagnostics view")
        result = run(executable, "inspect", str(mvr), "--view", "xml")
        require(result[0] == 0 and result[1] == MVR_XML, "exact XML output")
        result = run(executable, "inspect", str(gdtf), "--json")
        report = json.loads(result[1])
        require(result[0] == 0 and report["format"] == "GDTF" and report["document"]["root_xml"] == GDTF_XML,
                "complete GDTF JSON")
        result = run(executable, "inspect", str(compatibility), "--json")
        require(result[0] == 1 and json.loads(result[1])["status"] == "compatibility_accepted",
                "compatibility warning")
        result = run(executable, "inspect", str(unsafe_mvr), "--json")
        unsafe_report = json.loads(result[1])
        require(result[0] == 1 and not result[2], "unsafe-entry stderr cleanliness")
        require(any(item["code"] == "package.unsafe_entry_path"
                    for item in unsafe_report["diagnostics"]),
                "unsafe-entry structured diagnostic")
        result = run(executable, "inspect", str(malformed), "--json")
        require(result[0] == 3 and json.loads(result[1])["success"] is False and not result[2],
                "structured malformed source")
        result = run(executable, "inspect", str(unsupported))
        require(result[0] == 4 and not result[1] and "unsupported input type" in result[2],
                "unsupported input routing")
        result = run(executable, "inspect", str(unicode_mvr), "--json")
        unicode_source_path = json.loads(result[1])["request"]["source_path"]
        require(result[0] == 0 and Path(unicode_source_path).name == unicode_name
                and "�" not in unicode_source_path and "Ã" not in unicode_source_path,
                "Unicode filesystem path")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
