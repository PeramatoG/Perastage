#!/usr/bin/env python3
import sys
import zipfile
from pathlib import Path

FIXED_TIMESTAMP = (2026, 1, 1, 0, 0, 0)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: PerastageGenerateGdtfArchive.py <description.xml> <output.gdtf>", file=sys.stderr)
        return 2
    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    data = source.read_bytes()
    output.parent.mkdir(parents=True, exist_ok=True)
    info = zipfile.ZipInfo("description.xml", FIXED_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o644 << 16
    with zipfile.ZipFile(output, "w") as archive:
        archive.writestr(info, data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
