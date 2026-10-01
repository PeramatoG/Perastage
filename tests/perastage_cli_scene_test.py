#!/usr/bin/env python3
"""Exercise the real external scene workflow and its publication guarantees."""

from __future__ import annotations

import hashlib
import io
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


SCENE_XML = """<?xml version="1.0" encoding="UTF-8"?>
<GeneralSceneDescription verMajor="1" verMinor="6">
  <UserData/>
  <Scene><Layers><Layer uuid="11111111-1111-4111-8111-111111111111">
    <Name>Layer</Name><ChildList><Fixture uuid="22222222-2222-4222-8222-222222222222">
      <Name>Fixture</Name><Matrix>{1,0,0,0}{0,1,0,0}{0,0,1,0}{0,0,0,1}</Matrix>
      <FixtureID>1</FixtureID><FixtureIDNumeric>1</FixtureIDNumeric>
      <GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</GDTFMode>
    </Fixture></ChildList>
  </Layer></Layers></Scene>
</GeneralSceneDescription>
"""


def run(cli: Path, *args: str) -> subprocess.CompletedProcess[str]:
    """Run one CLI command with captured stable text streams."""
    return subprocess.run([str(cli), *args], text=True, capture_output=True)


def digest(path: Path) -> str:
    """Return a byte-for-byte source or destination fingerprint."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    """Verify mutation, ordering, resource retention, and failure safety."""
    cli = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        source = root / "source.mvr"
        output = root / "output.mvr"
        fixture_bytes = io.BytesIO()
        with zipfile.ZipFile(fixture_bytes, "w") as fixture_archive:
            fixture_archive.writestr(
                "description.xml",
                '<GDTF DataVersion="1.2"><FixtureType Name="Fixture" '
                'Manufacturer="Test" FixtureTypeID="33333333-3333-4333-8333-333333333333">'
                '<AttributeDefinitions/><Wheels/><PhysicalDescriptions/>'
                '<Models/><Geometries/><DMXModes><DMXMode Name="Mode" '
                'Geometry=""/></DMXModes></FixtureType></GDTF>')
        with zipfile.ZipFile(source, "w") as archive:
            archive.writestr("GeneralSceneDescription.xml", SCENE_XML)
            archive.writestr("fixture.gdtf", fixture_bytes.getvalue())
        source_digest = digest(source)

        result = run(cli, "scene", str(source), "--output", str(output),
                     "--command", "f 1", "--command", "pos x 1",
                     "--command", "pos y ++2")
        assert result.returncode == 0, result.stderr
        assert digest(source) == source_digest
        assert run(cli, "inspect", str(output), "--json").returncode in (0, 1)
        with zipfile.ZipFile(output) as archive:
            packaged_gdtfs = [name for name in archive.namelist()
                              if name.lower().endswith(".gdtf")]
            assert len(packaged_gdtfs) == 1
            assert archive.read(packaged_gdtfs[0])
            xml = archive.read("GeneralSceneDescription.xml").decode()
            assert "1000" in xml and "2000" in xml

        unchanged = output.read_bytes()
        assert run(cli, "scene", str(source), "--output", str(output),
                   "--command", "f 1").returncode != 0
        assert output.read_bytes() == unchanged
        failure = run(cli, "scene", str(source), "--output", str(output),
                      "--overwrite", "--command", "f 1 pos x 4",
                      "--command", "unknown")
        assert failure.returncode != 0
        assert output.read_bytes() == unchanged
        assert digest(source) == source_digest
        assert run(cli, "scene", str(source), "--command", "f 1").returncode == 2
        assert run(cli, "scene", str(source), "--output", str(source),
                   "--overwrite", "--command", "f 1").returncode != 0

        overwritten = run(cli, "scene", str(source), "--output", str(output),
                          "--overwrite", "--command", "f 1",
                          "--command", "rot z 90", "--json")
        assert overwritten.returncode == 0, overwritten.stderr
        assert '"output_published":true' in overwritten.stdout
        assert digest(source) == source_digest
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
