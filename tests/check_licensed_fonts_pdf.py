import pathlib
import subprocess
import sys
import tempfile


def run(*args):
    return subprocess.check_output(args, text=True, encoding="utf-8")


with tempfile.TemporaryDirectory() as temp:
    path = pathlib.Path(temp) / "licensed-fonts.pdf"
    run(sys.argv[1], "--licensed-fonts", str(path))
    fonts = run(sys.argv[2], str(path))
    rows = [row.split() for row in fonts.splitlines()[2:]]
    expected = {
        "SourceSans3-Bold",
        "SourceSans3-Regular",
        "NotoSerif-Regular",
        "NotoSerif-Bold",
        "NotoSans-Bold",
    }
    assert {row[0] for row in rows} == expected, fonts
    assert all(row[1:7] == ["CID", "TrueType", "Identity-H", "yes", "no", "yes"] for row in rows), fonts

    text = run(sys.argv[3], "-raw", str(path), "-")
    for content in (
        "Producción Málaga — Diseño",
        "Notas: € / Ω / Ж",
        "Fallback de proyecto",
        "Foco Escénico",
        "Teatro Español",
        "Lámpara Ágil",
    ):
        assert text.count(content) == 1, (content, text)
    assert "Pages:" in run(sys.argv[4], str(path))
