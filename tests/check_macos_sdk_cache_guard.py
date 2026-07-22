#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("guard", ROOT / ".github/scripts/macos_sdk_cache_guard.py")
guard = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(guard)


def assert_classifies(current: Path, refs: set[str], stale_count: int) -> None:
    _, _, stale = guard.classify_sdk_paths(current, refs)
    assert len(stale) == stale_count, stale


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="Perastage SDK Cache ") as tmp:
        root = Path(tmp)
        xcode = root / "Applications" / "Xcode 26.app" / "Contents" / "Developer" / "Platforms" / "MacOSX.platform" / "Developer" / "SDKs"
        xcode.mkdir(parents=True)
        versioned = xcode / "MacOSX26.5.sdk"
        versioned.mkdir()
        alias = xcode / "MacOSX.sdk"
        alias.symlink_to(versioned, target_is_directory=True)
        other = xcode / "MacOSX25.4.sdk"
        other.mkdir()

        assert_classifies(versioned, {str(versioned)}, 0)
        assert_classifies(versioned, {str(alias)}, 0)
        assert_classifies(versioned, {str(xcode / "MacOSX24.0.sdk")}, 1)
        assert_classifies(versioned, {str(other)}, 1)
        assert_classifies(versioned, {str(alias), str(root / "Applications" / "Xcode With Spaces.app" / "Contents" / "Developer" / "Platforms" / "MacOSX.platform" / "Developer" / "SDKs" / "MacOSX.sdk")}, 1)

        scan = root / "scan"
        scan.mkdir()
        metadata_path = "/Applications/Xcode_26.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk"
        (scan / "meta.txt").write_text(f"sdk={metadata_path}\n", encoding="utf-8")
        assert metadata_path in guard.discover_sdk_paths([scan])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
