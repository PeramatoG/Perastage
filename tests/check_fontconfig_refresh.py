"""Isolate Fontconfig's registrations from the host and exercise native refresh."""
import html
import os
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    discovery = root / "fonts"
    discovery.mkdir()
    config = root / "fonts.conf"
    config.write_text(
        '<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">\n'
        f"<fontconfig><dir>{html.escape(str(discovery))}</dir>"
        f"<cachedir>{html.escape(str(root / 'cache'))}</cachedir></fontconfig>\n",
        encoding="utf-8",
    )
    env = {**os.environ, "FONTCONFIG_FILE": str(config)}
    subprocess.run((sys.argv[1], str(discovery / "newly-installed.ttf")), check=True, env=env)
