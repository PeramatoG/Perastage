#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
import tempfile
import zipfile
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

        sentinel = root / "retained" / "sentinel"
        sentinel.parent.mkdir()
        sentinel.write_text("keep", encoding="utf-8")
        _, equivalent, stale = guard.classify_sdk_paths(versioned, {str(alias), str(versioned)})
        assert len(equivalent) == 2 and not stale
        assert sentinel.exists()

        scan = root / "scan"
        scan.mkdir()
        sdk_root = "/Applications/Xcode_26.6.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
        combined = (
            sdk_root + "/System/Library/Frameworks/OpenGL.framework;" + sdk_root
            + "/usr/include\0" + sdk_root
        )
        assert guard.extract_sdk_paths(combined) == {sdk_root}
        second = sdk_root.replace("Xcode_26.6", "Xcode_25.4").replace("MacOSX.sdk", "MacOSX25.4.sdk")
        quoted = f'"{sdk_root}/usr/include";\'{second}/System/Library/Frameworks/AppKit.framework\''
        assert guard.extract_sdk_paths(quoted) == {sdk_root, second}
        assert not guard.extract_sdk_paths(sdk_root.removesuffix(".sdk"))

        metadata = scan / "sample-config.cmake"
        metadata.write_text(f"set(CMAKE_OSX_SYSROOT \"{sdk_root}\")\n", encoding="utf-8")
        ignored_text = scan / "readme.txt"
        ignored_text.write_text(second, encoding="utf-8")
        binary = scan / "library.a"
        binary.write_bytes(b"archive\0" + second.encode())
        with zipfile.ZipFile(scan / "binary.zip", "w") as archive:
            archive.writestr("embedded.cmake", second)
        malformed = scan / "broken.pc"
        malformed.write_bytes(b"prefix=" + sdk_root.encode() + b"\xff")
        discovered = guard.discover_sdk_paths([scan])
        assert discovered == {sdk_root: {metadata}}, discovered

        retained_root = root / "retained-by-cli"
        retained_root.mkdir()
        (retained_root / "sentinel").write_text("keep", encoding="utf-8")
        retained_output = root / "retained-output"
        retained_environment = os.environ.copy()
        retained_environment["GITHUB_OUTPUT"] = str(retained_output)
        subprocess.run(
            [sys.executable, str(ROOT / ".github/scripts/macos_sdk_cache_guard.py"),
             "--current-sdk", str(alias), "--scan-root", str(root / "empty"),
             "--purge-root", str(retained_root)],
            check=True, env=retained_environment, capture_output=True, text=True,
        )
        assert (retained_root / "sentinel").exists()
        assert "guard-result=retained" in retained_output.read_text(encoding="utf-8")

        purge = root / "purge"
        purge.mkdir()
        (purge / "sentinel").write_text("delete", encoding="utf-8")
        output = root / "github-output"
        environment = os.environ.copy()
        environment["GITHUB_OUTPUT"] = str(output)
        subprocess.run(
            [sys.executable, str(ROOT / ".github/scripts/macos_sdk_cache_guard.py"),
             "--current-sdk", str(versioned), "--scan-root", str(scan),
             "--purge-root", str(purge)],
            check=True, env=environment, capture_output=True, text=True,
        )
        assert not (purge / "sentinel").exists()
        result = output.read_text(encoding="utf-8")
        assert "guard-result=invalidated" in result
        assert f"invalidation-source={metadata}" in result
        assert binary.exists() and (scan / "binary.zip").exists()

        summary = root / "summary.md"
        environment["GITHUB_STEP_SUMMARY"] = str(summary)
        subprocess.run(
            [sys.executable, str(ROOT / ".github/scripts/write_vcpkg_cache_summary.py"),
             "--platform", "macos-debug", "--triplet", "arm64-osx", "--baseline", "test",
             "--primary-key", "test-key", "--downloads-hit", "true", "--compiled-hit", "true",
             "--compiled-save-outcome", "skipped", "--sdk-guard-result", "invalidated",
             "--sdk-guard-reason", "incompatible SDK metadata", "--sdk-invalidation-source", str(metadata)],
            check=True, env=environment,
        )
        summary_text = summary.read_text(encoding="utf-8")
        assert "Effective compiled cache reuse: no, restored cache was invalidated" in summary_text
        assert f"macOS SDK invalidation source: {metadata}" in summary_text
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
