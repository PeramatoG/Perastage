#!/usr/bin/env python3
"""Regenerate checked-in PNG variants once from original repository artwork."""
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
RESOURCES = HERE.parents[2] / "resources"


def main() -> None:
    """Pillow is maintainer-only; packaging and CI consume the committed assets."""
    assets = HERE / "assets"
    assets.mkdir(exist_ok=True)
    with Image.open(RESOURCES / "Perastage_logo_1024.png") as image:
        source = image.convert("RGBA")
    for name, size in (("StoreLogo.png", 50), ("Square44x44Logo.png", 44),
                       ("Square150x150Logo.png", 150)):
        source.resize((size, size), Image.Resampling.LANCZOS).save(assets / name)
    for icon, name in (("PerastageProject.ico", "ProjectFile.png"), ("PerastageMVR.ico", "MvrFile.png")):
        with Image.open(RESOURCES / icon) as image:
            source = image.ico.getimage(max(image.ico.sizes(), key=lambda size: size[0])).convert("RGBA")
        source.resize((32, 32), Image.Resampling.LANCZOS).save(assets / name)


if __name__ == "__main__":
    main()
