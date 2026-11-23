"""Generate the original geometric fonts for the system-font regression tests.

Run with FontTools. The resulting fixtures and this generator are licensed CC0 1.0.
The application does not package them or require FontTools.
"""
from pathlib import Path

from fontTools.designspaceLib import AxisDescriptor, DesignSpaceDocument, SourceDescriptor
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTCollection, TTFont
from fontTools.varLib import build

ROOT = Path(__file__).resolve().parent
CHARS = [*range(32, 127), 0xE9, 0x301]


def make_font(path, family, postscript, *, cff=False, weight=400):
    builder = FontBuilder(1000, isTTF=not cff)
    cmap = {cp: f"uni{cp:04X}" for cp in CHARS}
    names = [".notdef", *cmap.values()]
    builder.setupGlyphOrder(names)
    builder.setupCharacterMap(cmap)
    glyphs = {}
    metrics = {}
    for index, name in enumerate(names):
        advance = 0 if name == "uni0301" else 430 + (index % 7) * 30 + (35 if weight >= 700 else 0)
        pen = T2CharStringPen(advance, None) if cff else TTGlyphPen(None)
        if name != "uni0020":
            x = 15 + (index % 3) * 10
            y = 540 if name == "uni0301" else 0
            pen.moveTo((x, y))
            pen.lineTo((250, y))
            pen.lineTo((220, y + 470 if not y else y + 70))
            pen.lineTo((x, y + 300 if not y else y + 40))
            pen.closePath()
        glyphs[name] = pen.getCharString() if cff else pen.glyph()
        metrics[name] = (advance, 0 if name == "uni0020" else x)
    if cff:
        builder.setupCFF(postscript, {"FullName": f"{family} Regular",
                                     "FamilyName": family, "Weight": "Regular"},
                         glyphs, {})
    else:
        builder.setupGlyf(glyphs)
    builder.setupHorizontalMetrics(metrics)
    builder.setupHorizontalHeader(ascent=800, descent=-200)
    style = "Bold" if weight >= 700 else "Regular"
    builder.setupNameTable({"familyName": family, "styleName": style,
                            "psName": postscript, "uniqueFontIdentifier": postscript,
                            "fullName": f"{family} {style}", "version": "Version 1.000"})
    builder.setupOS2(version=4, usWeightClass=weight, fsSelection=0x20 if weight >= 700 else 0x40,
                     fsType=0, sTypoAscender=800, sTypoDescender=-200,
                     usWinAscent=800, usWinDescent=200, sCapHeight=470)
    builder.setupHead(created=3786912000, modified=3786912000, macStyle=int(weight >= 700))
    builder.setupPost()
    builder.font.recalcTimestamp = False
    builder.save(path)


make_font(ROOT / "PerastageTestSans.ttf", "Perastage Test Sans", "PerastageTestSans-Regular")
make_font(ROOT / "PerastageTestBold.tmp", "Perastage Test Sans", "PerastageTestSans-Bold", weight=700)
make_font(ROOT / "PerastageTestCff.otf", "Perastage Test Cff", "PerastageTestCff-Regular", cff=True)
collection = TTCollection()
collection.fonts = [TTFont(ROOT / name) for name in ("PerastageTestSans.ttf", "PerastageTestBold.tmp")]
collection.save(ROOT / "PerastageTestCollection.ttc")
for font in collection.fonts:
    font.close()
design = DesignSpaceDocument()
axis = AxisDescriptor()
axis.name, axis.tag = "Weight", "wght"
axis.minimum, axis.default, axis.maximum = 400, 400, 700
design.addAxis(axis)
for filename, style, weight in [("PerastageTestSans.ttf", "Regular", 400),
                                 ("PerastageTestBold.tmp", "Bold", 700)]:
    source = SourceDescriptor()
    source.path = str(ROOT / filename)
    source.name, source.familyName, source.styleName = style, "Perastage Test Sans", style
    source.location = {"Weight": weight}
    design.addSource(source)
variable, _, _ = build(design)
variable.recalcTimestamp = False
variable.save(ROOT / "PerastageTestVariable.ttf")
(ROOT / "PerastageTestBold.tmp").unlink()
