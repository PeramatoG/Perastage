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
        "NotoSansArabic-Bold",
        "NotoSansArabic-Regular",
        "NotoSerif-BoldItalic",
        "SourceSans3-It",
        "NotoSansHebrew-Regular",
        "NotoSansMono-Regular",
        "NotoSerif-Italic",
        "SourceSans3-BoldIt",
    }
    assert {row[0] for row in rows} == expected, fonts
    assert all(row[1:7] == ["CID", "TrueType", "Identity-H", "yes", "no", "yes"] for row in rows), fonts

    rtl_text = (
        ("Scene 12 — שלום", "Scene 12 — םולש"),
        ("Fixture 24 — مرحبا", "Fixture 24 — ابحرم"),
        ("Rack12 שלום", "Rack12 םולש"),
        ("Lamp24 مرحبا", "Lamp24 ابحرم"),
    )
    # Normal extraction must recover logical text, including RTL ordering.
    # Raw mode can expose the drawing order on some Poppler releases.
    for args in ((), ("-raw",)):
        # Windows Poppler can use the console code page for standard output.
        extracted = path.with_suffix(".txt")
        run(sys.argv[3], "-enc", "UTF-8", *args, str(path), str(extracted))
        # Remove only the directional embedding controls added by Poppler.
        text = extracted.read_text(encoding="utf-8").translate(dict.fromkeys(map(ord, "\u202a\u202b\u202c")))
        for content in (
            "Producción Málaga — Diseño",
            "Notas: € / Ω / Ж",
            "Fallback de proyecto",
            "Foco Escénico",
            "Teatro Español",
            "Lámpara Ágil",
            "x́ ffi",
            "Canvas Serif",
            "Canvas Source",
            "AV To fi ffi é é Å Å x́",
            "España · Iluminación · À È É Ñ Ü ß € £ © ™",
            "Αθήνα — Москва — Журнал · 🙼",
            "A B C",
            "Unsupported: 灯 😀",
        ):
            assert text.count(content) == 1, (content, args, text)
        for logical, visual in rtl_text:
            count = text.count(logical) + (text.count(visual) if args else 0)
            assert count == 1, (logical, args, text)
        if not args:
            # Raw mode can put the whole words of an RTL phrase in drawing order.
            for logical in ("مرحبا عالم", "שלום עולם"):
                assert text.count(logical) == 1, (logical, text)
    assert "Pages:" in run(sys.argv[4], str(path))
