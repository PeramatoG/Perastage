#!/usr/bin/env python3
import sys
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path

EXPECTED_ID = "11111111-2222-4333-8444-555555555555"
FIXED_TIMESTAMP = (2026, 1, 1, 0, 0, 0)
EXPECTED_MODE = (0o100644 << 16)


def fail(message: str) -> int:
    print(f"ERROR: {message}", file=sys.stderr)
    return 1


def is_secure_relative_path(name: str) -> bool:
    parts = Path(name).parts
    return bool(name) and not name.startswith(("/", "\\")) and "\\" not in name and ":" not in name and ".." not in parts and all(part not in ("", ".") for part in parts)


def read_description(archive: Path):
    if not archive.is_file():
        return None, fail(f"missing generated fallback archive: {archive}")
    try:
        with zipfile.ZipFile(archive) as zf:
            names = zf.namelist()
            if names != ["description.xml"]:
                return None, fail(f"unexpected fallback archive entries: {names}")
            info = zf.getinfo("description.xml")
            if not is_secure_relative_path(info.filename):
                return None, fail(f"unsafe archive entry: {info.filename}")
            if info.date_time != FIXED_TIMESTAMP:
                return None, fail(f"description.xml timestamp is not deterministic: {info.date_time}")
            if info.compress_type != zipfile.ZIP_STORED:
                return None, fail("description.xml is not stored with ZIP_STORED")
            if info.create_system != 3:
                return None, fail(f"description.xml create_system is not Unix metadata: {info.create_system}")
            if info.external_attr != EXPECTED_MODE:
                return None, fail(f"description.xml external_attr is not fixed: {info.external_attr:#x}")
            if zf.comment:
                return None, fail("archive comment must be empty")
            return zf.read("description.xml"), 0
    except zipfile.BadZipFile as exc:
        return None, fail(f"invalid ZIP archive: {exc}")


def validate_xml(xml: bytes) -> int:
    try:
        root = ET.fromstring(xml)
    except ET.ParseError as exc:
        return fail(f"description.xml parse failed: {exc}")
    if root.attrib.get("DataVersion") != "1.2":
        return fail("fallback DataVersion is not 1.2")
    fixture = root.find("FixtureType")
    if fixture is None or fixture.attrib.get("FixtureTypeID") != EXPECTED_ID:
        return fail("fallback FixtureTypeID is not canonical")
    for section in ["AttributeDefinitions", "Wheels", "PhysicalDescriptions", "Models", "Geometries", "DMXModes"]:
        if fixture.find(section) is None:
            return fail(f"missing mandatory section: {section}")
    mode = fixture.find("DMXModes/DMXMode")
    if mode is None or mode.attrib.get("Name") != "Default" or mode.attrib.get("Geometry") != "Root":
        return fail("default DMX mode does not coherently reference Root geometry")
    channels = fixture.findall("DMXModes/DMXMode/DMXChannels/DMXChannel")
    if len(channels) != 1 or channels[0].attrib.get("Offset") != "1" or channels[0].attrib.get("Geometry") != "Root":
        return fail("fallback is not exactly one physical DMX address")
    logical = channels[0].find("LogicalChannel")
    function = logical.find("ChannelFunction") if logical is not None else None
    if logical is None or logical.attrib.get("Attribute") != "Dimmer" or function is None or function.attrib.get("Attribute") != "Dimmer" or function.attrib.get("DMXFrom") != "0/1":
        return fail("fallback Dimmer channel references are incoherent")
    return 0


def main() -> int:
    if len(sys.argv) != 2:
        return fail("usage: check_dummy_gdtf_fallback.py <Dummy 1ch.gdtf>")
    xml, code = read_description(Path(sys.argv[1]))
    if code:
        return code
    return validate_xml(xml)


if __name__ == "__main__":
    raise SystemExit(main())
