#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
archive="${1:?expected generated Dummy 1ch.gdtf path}"
staged_root="${2:-}"
installed_root="${3:-}"
run_test_python - "$archive" "$staged_root" "$installed_root" <<'PY'
import sys, zipfile, xml.etree.ElementTree as ET
from pathlib import Path
archive = Path(sys.argv[1])
staged = Path(sys.argv[2]) if sys.argv[2] else None
installed = Path(sys.argv[3]) if sys.argv[3] else None
expected_id = "11111111-2222-4333-8444-555555555555"
if not archive.is_file():
    raise SystemExit(f"missing generated fallback archive: {archive}")
with zipfile.ZipFile(archive) as zf:
    names = zf.namelist()
    if names != ["description.xml"]:
        raise SystemExit(f"unexpected fallback archive entries: {names}")
    for name in names:
        parts = Path(name).parts
        if name.startswith(("/", "\\")) or "\\" in name or ".." in parts or any(part in ("", ".") for part in parts):
            raise SystemExit(f"unsafe archive entry: {name}")
    root = ET.fromstring(zf.read("description.xml"))
if root.attrib.get("DataVersion") != "1.2":
    raise SystemExit("fallback DataVersion is not 1.2")
fixture = root.find("FixtureType")
if fixture is None or fixture.attrib.get("FixtureTypeID") != expected_id:
    raise SystemExit("fallback FixtureTypeID is not canonical")
for section in ["AttributeDefinitions", "Wheels", "PhysicalDescriptions", "Models", "Geometries", "DMXModes"]:
    if fixture.find(section) is None:
        raise SystemExit(f"missing mandatory section: {section}")
mode = fixture.find("DMXModes/DMXMode")
if mode is None or mode.attrib.get("Geometry") != "Root":
    raise SystemExit("default DMX mode does not reference Root geometry")
channels = fixture.findall("DMXModes/DMXMode/DMXChannels/DMXChannel")
if len(channels) != 1 or channels[0].attrib.get("Offset") != "1":
    raise SystemExit("fallback is not exactly one physical DMX address")
logical = channels[0].find("LogicalChannel")
function = logical.find("ChannelFunction") if logical is not None else None
if logical is None or logical.attrib.get("Attribute") != "Dimmer" or function is None or function.attrib.get("DMXFrom") != "0/1":
    raise SystemExit("fallback Dimmer channel references are incoherent")
for root_dir, label in [(staged, "staged"), (installed, "installed")]:
    if not root_dir:
        continue
    fallback = root_dir / "library" / "fixtures" / "Dummy 1ch.gdtf"
    source_xml = root_dir / "library" / "fixtures" / "Dummy 1ch.description.xml"
    if not fallback.is_file():
        raise SystemExit(f"{label} fallback archive is missing: {fallback}")
    if source_xml.exists():
        raise SystemExit(f"{label} source XML must not be installed: {source_xml}")
PY
