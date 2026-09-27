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


def require_automation_contract(report: object, expected_format: str) -> dict[str, object]:
    """Check the stable minimum while allowing additive report members."""
    require(isinstance(report, dict), "JSON report object")
    required = {
        "schema_version", "request", "success", "worst_severity", "diagnostics",
        "format", "status", "package", "resources", "validation",
    }
    require(required <= report.keys(), "minimum top-level automation fields")
    require(report["schema_version"] == 1
            and type(report["schema_version"]) is int, "schema version contract")
    require(report["format"] == expected_format, "format token")
    require(type(report["success"]) is bool, "success value type")
    require(report["worst_severity"] is None
            or report["worst_severity"] in {"information", "warning", "error", "fatal"},
            "worst-severity token")
    require(isinstance(report["request"], dict)
            and isinstance(report["request"].get("source_path"), str),
            "request source-path contract")
    require(isinstance(report["diagnostics"], list)
            and isinstance(report["resources"], list)
            and isinstance(report["validation"], list), "stable collection types")
    return report


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
        report = require_automation_contract(json.loads(result[1]), "GDTF")
        require(result[0] == 0 and not result[2]
                and isinstance(report.get("document"), dict)
                and report["document"]["root_xml"] == GDTF_XML,
                "complete GDTF JSON")
        result = run(executable, "inspect", str(compatibility), "--json")
        compatibility_report = require_automation_contract(json.loads(result[1]), "GDTF")
        require(result[0] == 1 and not result[2]
                and compatibility_report["status"] == "compatibility_accepted",
                "compatibility warning")
        result = run(executable, "inspect", str(unsafe_mvr), "--json")
        unsafe_report = require_automation_contract(json.loads(result[1]), "MVR")
        require(result[0] == 1 and not result[2], "unsafe-entry stderr cleanliness")
        unsafe_diagnostic = next((item for item in unsafe_report["diagnostics"]
                                  if item["code"] == "package.unsafe_entry_path"), None)
        require(unsafe_diagnostic is not None
                and unsafe_diagnostic["severity"] == "error"
                and unsafe_diagnostic["domain"] == "package"
                and unsafe_diagnostic["classification"] == "general",
                "unsafe-entry stable diagnostic tokens")
        location = unsafe_diagnostic.get("location", {})
        require(Path(location.get("source_path", "")).name == unsafe_mvr.name
                and location.get("package_entry") == "../unsafe-resource.txt",
                "unsafe-entry structured diagnostic location")
        result = run(executable, "inspect", str(malformed), "--json")
        malformed_report = require_automation_contract(json.loads(result[1]), "MVR")
        require(result[0] == 3 and malformed_report["success"] is False and not result[2],
                "structured malformed source")
        result = run(executable, "inspect", str(unsupported))
        require(result[0] == 4 and not result[1] and "unsupported input type" in result[2],
                "unsupported input routing")
        result = run(executable, "inspect")
        require(result[0] == 2 and not result[1]
                and "exactly one input file" in result[2], "usage failure routing")
        result = run(executable, "inspect", str(unicode_mvr), "--json")
        unicode_report = require_automation_contract(json.loads(result[1]), "MVR")
        unicode_source_path = unicode_report["request"]["source_path"]
        require(result[0] == 0 and not result[2]
                and Path(unicode_source_path).name == unicode_name
                and "�" not in unicode_source_path and "Ã" not in unicode_source_path,
                "Unicode filesystem path")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
