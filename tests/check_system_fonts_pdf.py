"""Validate an installed-font PDF independently of the Perastage exporter."""
import pathlib
import re
import struct
import subprocess
import sys
import tempfile
import zlib


def run(*args):
    return subprocess.check_output(args, encoding="utf-8", stderr=subprocess.PIPE)


def embedded_programs(pdf):
    for reference in re.findall(rb"/FontFile2 ([0-9]+) 0 R", pdf):
        entry = re.search(rb"\n" + reference + rb" 0 obj\n(.*?)stream\n", pdf, re.S)
        length = int(re.search(rb"/Length ([0-9]+)", entry[1])[1])
        program = pdf[entry.end():entry.end() + length]
        if b"/FlateDecode" in entry[1]:
            program = zlib.decompress(program)
        yield program


def installed_variants(programs):
    fixture = (pathlib.Path(__file__).parent / "fixtures/system_fonts/PerastageTestSans.ttf").read_bytes()
    for program in programs:
        table_count = struct.unpack_from(">H", program, 4)[0]
        for index in range(table_count):
            tag, _, offset, _ = struct.unpack_from(">4sIII", program, 12 + index * 16)
            if tag == b"OS/2":
                flags = struct.unpack_from(">H", program, offset + 8)[0]
                if len(program) == len(fixture):
                    assert program[:offset + 8] == fixture[:offset + 8]
                    assert program[offset + 10:] == fixture[offset + 10:]
                yield flags


with tempfile.TemporaryDirectory() as temp:
    pdf = pathlib.Path(temp) / "installed.pdf"
    run(sys.argv[1], "--system-fonts", str(pdf))
    fonts = run(sys.argv[2], str(pdf))
    records = [line for line in fonts.splitlines() if "PerastageTestSans-Regular" in line]
    assert len(records) == 2 and all(re.search(r"CID TrueType\s+Identity-H\s+yes\s+no\s+yes", row)
                                      for row in records), fonts
    assert "PerastageTestSans-Bold" not in fonts and "PerastageTestCff" not in fonts, fonts
    rights = set(installed_variants(embedded_programs(pdf.read_bytes())))
    assert rights == {0, 0x108}, rights
    text = run(sys.argv[3], "-enc", "UTF-8", str(pdf), "-").translate(dict.fromkeys(range(0x202A, 0x202F)))
    for expected in ("External café", "Restricted x́ ffi", "CFF fallback", "Print fallback",
                     "Editable full", "Collection fallback", "External event", "External legend", "Gate12 مرحبا", "Canvas CFF"):
        assert expected in text, (expected, text)
    assert "Pages:" in run(sys.argv[4], str(pdf))
