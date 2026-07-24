import subprocess
import sys
import zipfile
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / ".github" / "scripts" / "assemble_debug_symbols.py"


def make_artifacts(root: Path) -> None:
    (root / "Perastage-windows-symbols").mkdir()
    (root / "Perastage-windows-symbols" / "Perastage.pdb").write_text("pdb")
    linux = root / "Perastage-linux-symbols"
    linux.mkdir()
    with zipfile.ZipFile(linux / "Perastage-1.5.0-Linux-symbols.zip", "w") as archive:
        archive.writestr("Perastage.debug", "debug")
    arch = root / "Perastage-archlinux-symbols"
    arch.mkdir()
    with zipfile.ZipFile(arch / "Perastage-1.5.0-ArchLinux-symbols.zip", "w") as archive:
        archive.writestr("perastage-debug-1.5.0-1-x86_64.pkg.tar.zst", "archdebug")
    for name in ["Perastage-macos15-symbols", "Perastage-macos26-symbols"]:
        dwarf = root / name / "Perastage.dSYM" / "Contents" / "Resources" / "DWARF"
        dwarf.mkdir(parents=True)
        (dwarf / "Perastage").write_text(name)


def run_assembler(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--artifacts", str(root), "--version", "1.5.0", "--output-dir", str(root / "final")],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def test_assembler_copies_real_symbols_and_bundles(tmp_path: Path) -> None:
    make_artifacts(tmp_path)
    result = run_assembler(tmp_path)
    assert result.returncode == 0, result.stderr
    final_zip = tmp_path / "final" / "Perastage-1.5.0-Debug-Symbols-Developers-Only.zip"
    with zipfile.ZipFile(final_zip) as archive:
        names = set(archive.namelist())
    assert "README.txt" in names
    assert "Windows-x64/Perastage.pdb" in names
    assert "Linux-AppImage-x86_64/Perastage.debug" in names
    assert "ArchLinux-x86_64/perastage-debug-1.5.0-1-x86_64.pkg.tar.zst" in names
    assert "macOS15-arm64/Perastage.dSYM/Contents/Resources/DWARF/Perastage" in names
    assert "macOS26-arm64/Perastage.dSYM/Contents/Resources/DWARF/Perastage" in names
    assert "symbol-files.txt" not in names


def test_assembler_rejects_missing_platform_symbols(tmp_path: Path) -> None:
    make_artifacts(tmp_path)
    (tmp_path / "Perastage-linux-symbols" / "Perastage-1.5.0-Linux-symbols.zip").unlink()
    result = run_assembler(tmp_path)
    assert result.returncode != 0
    assert "Linux-AppImage_x86_64" not in result.stderr
    assert "Linux-AppImage-x86_64" in result.stderr
