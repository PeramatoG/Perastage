import json
import subprocess
import sys
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / ".github" / "scripts" / "validate_final_release_assets.py"
CONTRACT = Path(__file__).resolve().parents[2] / ".github" / "release-artifact-contract.json"
LEGACY_SHA = "c857665b99aacf9f466edd4416584dfb56ac1a1f"


def make_assets(root: Path, version: str = "1.5.0", provenance: bool = True, sha: str = LEGACY_SHA) -> None:
    names = [
        f"Perastage_{version}_Setup.exe",
        f"Perastage-{version}-x86_64.AppImage",
        f"Perastage-{version}-macOS15-arm64.dmg",
        f"Perastage-{version}-macOS26-arm64.dmg",
        f"Perastage-{version}-arch-x86_64.pkg.tar.zst",
        f"Perastage-{version}-Debug-Symbols-Developers-Only.zip",
    ]
    for name in names:
        (root / name).write_text(name)
    if provenance:
        (root / "release-provenance.json").write_text(json.dumps({"release_sha": sha, "release_version": version, "tag": f"v{version}"}))


def run_validator(root: Path, version: str = "1.5.0", *extra: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--assets-dir", str(root), "--version", version, "--contract", str(CONTRACT), *extra],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def test_validator_accepts_complete_assets_and_generates_provenance(tmp_path: Path) -> None:
    make_assets(tmp_path, provenance=False)
    checksums = tmp_path / "SHA256SUMS.txt"
    provenance = tmp_path / "release-provenance.json"
    result = run_validator(tmp_path, "1.5.0", "--checksums-output", str(checksums), "--provenance-output", str(provenance), "--release-sha", LEGACY_SHA)
    assert result.returncode == 0, result.stderr
    data = json.loads(provenance.read_text())
    assert data["release_sha"] == LEGACY_SHA
    assert data["release_version"] == "1.5.0"
    assert len(data["final_asset_filenames"]) == 6
    assert checksums.read_text().count("Perastage-") >= 5


def test_validator_rejects_missing_duplicate_empty_stale_and_unexpected_assets(tmp_path: Path) -> None:
    make_assets(tmp_path)
    (tmp_path / "Perastage-1.5.0-x86_64.AppImage").unlink()
    (tmp_path / "Perastage-1.5.0-copy-macOS15-arm64.dmg").write_text("dup")
    (tmp_path / "Perastage-1.5.0-macOS26-arm64.dmg").write_text("")
    (tmp_path / "Perastage_1.4.0_Setup.exe").write_text("stale")
    (tmp_path / "Other-1.5.0.dmg").write_text("unexpected")
    result = run_validator(tmp_path)
    assert result.returncode != 0
    assert "linux count 0" in result.stderr
    assert "macos15 count 2" in result.stderr
    assert "windows count 2" in result.stderr
    assert "Empty release asset" in result.stderr
    assert "Unexpected release asset" in result.stderr


def test_validator_legacy_provenance_exception_is_exact_pair_only(tmp_path: Path) -> None:
    make_assets(tmp_path, provenance=False)
    ok = run_validator(tmp_path, "1.5.0", "--validate-provenance", "--release-sha", LEGACY_SHA)
    assert ok.returncode == 0, ok.stderr
    bad = run_validator(tmp_path, "1.5.0", "--validate-provenance", "--release-sha", "0" * 40)
    assert bad.returncode != 0
    assert "Missing release-provenance.json" in bad.stderr
