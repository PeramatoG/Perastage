#!/usr/bin/env python3
"""Exercise deterministic vcpkg notice collection with a fake installed tree."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    """Verify notice copying, safe names, stale cleanup, and absent-tree handling."""
    with tempfile.TemporaryDirectory() as temporary:
        temp = Path(temporary)
        installed = temp / "installed"
        share = installed / "test-triplet" / "share"
        (share / "zlib").mkdir(parents=True)
        (share / "odd+port").mkdir()
        (share / "zlib" / "copyright").write_text("zlib notice\n", encoding="utf-8")
        (share / "odd+port" / "copyright").write_text("odd notice\n", encoding="utf-8")
        output = temp / "generated"
        output.mkdir()
        (output / "stale.txt").write_text("stale\n", encoding="utf-8")
        script = temp / "collect.cmake"
        script.write_text(
            f'include("{ROOT / "cmake/PerastageVcpkgNotices.cmake"}")\n'
            f'perastage_collect_vcpkg_notices("{installed}" "test-triplet" "{output}")\n',
            encoding="utf-8",
        )
        subprocess.run(["cmake", "-P", str(script)], check=True)
        actual = {path.name: path.read_text(encoding="utf-8") for path in output.iterdir()}
        expected = {"odd_port.txt": "odd notice\n", "zlib.txt": "zlib notice\n"}
        if actual != expected:
            raise SystemExit(f"unexpected generated notices: {actual!r}")

        missing_output = temp / "missing-generated"
        missing_script = temp / "missing.cmake"
        missing_script.write_text(
            f'include("{ROOT / "cmake/PerastageVcpkgNotices.cmake"}")\n'
            f'perastage_collect_vcpkg_notices("{temp / "absent"}" "triplet" "{missing_output}")\n',
            encoding="utf-8",
        )
        subprocess.run(["cmake", "-P", str(missing_script)], check=True)
        if missing_output.exists():
            raise SystemExit("an absent vcpkg share tree created output")
    print("vcpkg notice generation check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
