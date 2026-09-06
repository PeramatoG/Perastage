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
DIAGNOSTIC_SPEC = importlib.util.spec_from_file_location(
    "vcpkg_cache_diagnostics", ROOT / ".github/scripts/vcpkg_cache_diagnostics.py"
)
diagnostics = importlib.util.module_from_spec(DIAGNOSTIC_SPEC)
assert DIAGNOSTIC_SPEC.loader is not None
DIAGNOSTIC_SPEC.loader.exec_module(diagnostics)


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
        install_log = root / "vcpkg-install.log"
        install_log.write_text(
            "Restored 0 package(s) from local cache\n"
            "Building zlib:arm64-osx@1.3.2...\n"
            "Building vcpkg-cmake-config:arm64-osx@2024-05-23...\n",
            encoding="utf-8",
        )
        environment["GITHUB_STEP_SUMMARY"] = str(summary)
        subprocess.run(
            [sys.executable, str(ROOT / ".github/scripts/write_vcpkg_cache_summary.py"),
             "--platform", "macos-debug", "--triplet", "arm64-osx", "--baseline", "test",
             "--primary-key", "test-key", "--downloads-hit", "true", "--compiled-hit", "true",
             "--compiled-save-outcome", "skipped", "--sdk-guard-result", "retained",
             "--sdk-guard-reason", "compatible SDK metadata", "--sdk-invalidation-source", "",
             "--install-log", str(install_log)],
            check=True, env=environment,
        )
        summary_text = summary.read_text(encoding="utf-8")
        assert "Effective compiled cache reuse: no" in summary_text
        assert "Binary packages restored: 0" in summary_text
        assert "Source packages rebuilt: 2" in summary_text
        reused_log = root / "vcpkg-reused.log"
        reused_log.write_text("All requested installations are currently installed.\n", encoding="utf-8")
        assert diagnostics is not None
        summary_spec = importlib.util.spec_from_file_location(
            "vcpkg_summary", ROOT / ".github/scripts/write_vcpkg_cache_summary.py"
        )
        summary_module = importlib.util.module_from_spec(summary_spec)
        assert summary_spec.loader is not None
        summary_spec.loader.exec_module(summary_module)
        assert summary_module.parse_install_activity(reused_log) == ("yes", "unknown", "0")
        assert summary_module.publication_status("success", "success", False, "44", "v4-key") == (
            "yes, saved under `v4-key`"
        )
        assert summary_module.publication_status("success", "skipped", True, "0", "v4-key") == (
            "not needed, repaired primary cache was an exact hit"
        )
        assert summary_module.publication_status("failure", "success", False, "unknown", "v4-key") == (
            "no, dependency installation failed"
        )

        packages = root / "packages"
        abi = packages / "zlib_arm64-osx" / "vcpkg_abi_info.txt"
        abi.parent.mkdir(parents=True)
        abi.write_text("abi abc123\ntriplet arm64-osx\ncompiler_hash def456\n", encoding="utf-8")
        binary_root = root / "binary"
        binary_root.mkdir()
        (binary_root / "zlib_arm64-osx.1.3.2-vcpkgabc123.zip").write_bytes(b"zip")
        args = type("Args", (), {
            "label": "after-install", "installed_root": root / "installed",
            "packages_root": packages, "binary_root": binary_root, "triplet": "arm64-osx",
            "baseline": "baseline", "compiler": "clang", "sdk": "sdk", "vcpkg_version": "vcpkg",
        })()
        captured = diagnostics.capture(args)
        assert captured["binary_archive_count"] == 1
        assert captured["binary_archive_identifiers"] == ["zlib_arm64-osx.1.3.2-vcpkgabc123.zip"]
        assert "abi abc123" in captured["abi_representatives"]["zlib"][0]
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
