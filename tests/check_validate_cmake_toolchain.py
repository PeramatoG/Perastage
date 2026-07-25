#!/usr/bin/env python3
"""Exercise strict compiler and launcher validation across CI platforms."""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / ".github/scripts/validate_cmake_toolchain.py"


def write_layout(
    root: Path,
    compiler_id: str,
    compiler: str,
    launcher: Path,
    *,
    architecture: str = "",
    omit_launcher: str = "",
    launcher_override: Path | None = None,
    bash: Path | None = None,
) -> None:
    version = "4.4.0"
    (root / "CMakeFiles" / version).mkdir(parents=True)
    selected = launcher_override or launcher
    cache = [
        "CMAKE_GENERATOR:INTERNAL=Ninja",
        "CMAKE_BUILD_TYPE:STRING=Debug",
        f"CMAKE_C_COMPILER:FILEPATH={compiler}",
        f"CMAKE_CXX_COMPILER:FILEPATH={compiler}",
        "PERASTAGE_ENABLE_COMPILER_CACHE:BOOL=ON",
        f"PERASTAGE_COMPILER_CACHE_PROGRAM:FILEPATH={launcher}",
    ]
    if omit_launcher != "C":
        cache.append(f"CMAKE_C_COMPILER_LAUNCHER:STRING={selected}")
    if omit_launcher != "CXX":
        cache.append(f"CMAKE_CXX_COMPILER_LAUNCHER:STRING={selected}")
    if bash:
        cache.append(f"BASH_EXECUTABLE:FILEPATH={bash}")
    (root / "CMakeCache.txt").write_text("\n".join(cache), encoding="utf-8")
    for language in ("C", "CXX"):
        (root / "CMakeFiles" / version / f"CMake{language}Compiler.cmake").write_text(
            "\n".join([
                f'set(CMAKE_{language}_COMPILER_ID "{compiler_id}")',
                f'set(CMAKE_{language}_COMPILER_ARCHITECTURE_ID "{architecture}")',
            ]),
            encoding="utf-8",
        )


def run(build: Path, launcher: Path, compiler_id: str, *extra: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(TOOL), "--build-dir", str(build), "--expected-c-id", compiler_id,
         "--expected-cxx-id", compiler_id, "--expected-generator", "Ninja", "--expected-build-type", "Debug",
         "--expected-launcher", str(launcher), *extra],
        text=True,
        capture_output=True,
    )


with tempfile.TemporaryDirectory() as directory:
    base = Path(directory)
    launchers = {
        "linux": base / "linux" / "sccache",
        "macos": base / "mac os tools" / "sccache",
        "windows": base / "Windows Tools" / "sccache.exe",
    }
    for launcher in launchers.values():
        launcher.parent.mkdir(parents=True)
        launcher.write_text("executable", encoding="utf-8")

    cases = [
        ("linux", "GNU", "/usr/bin/gcc", []),
        ("macos", "AppleClang", "/usr/bin/clang", []),
        ("windows", "MSVC", "C:/Program Files/Microsoft Visual Studio/VC/Tools/MSVC/14.44/bin/Hostx64/x64/cl.exe",
         ["--expected-compiler-path-regex", r"Hostx64/x64", "--expected-architecture", "x64"]),
    ]
    for platform, compiler_id, compiler, extra in cases:
        build = base / f"build {platform}"
        write_layout(build, compiler_id, compiler, launchers[platform], architecture="x64" if platform == "windows" else "")
        result = run(build, launchers[platform], compiler_id, *extra)
        assert result.returncode == 0, result.stderr + result.stdout
        if platform == "windows":
            assert "sccache.exe" in (build / "CMakeCache.txt").read_text(encoding="utf-8")

    for missing in ("C", "CXX"):
        build = base / f"missing {missing}"
        write_layout(build, "GNU", "/usr/bin/gcc", launchers["linux"], omit_launcher=missing)
        result = run(build, launchers["linux"], "GNU")
        assert result.returncode != 0 and f"CMAKE_{missing}_COMPILER_LAUNCHER is missing" in result.stderr

    other = base / "other-sccache"
    other.write_text("other", encoding="utf-8")
    mismatch = base / "mismatched launcher"
    write_layout(mismatch, "GNU", "/usr/bin/gcc", launchers["linux"], launcher_override=other)
    assert run(mismatch, launchers["linux"], "GNU").returncode != 0

    truncated = base / "truncated Windows extension"
    truncated_launcher = launchers["windows"].with_suffix("")
    truncated_launcher.write_text("truncated", encoding="utf-8")
    write_layout(truncated, "MSVC", "C:/VS/Hostx64/x64/cl.exe", launchers["windows"],
                 architecture="x64", launcher_override=truncated_launcher)
    assert run(truncated, launchers["windows"], "MSVC").returncode != 0

    missing_executable = base / "not-present" / "sccache.exe"
    build = base / "missing executable"
    write_layout(build, "MSVC", "C:/VS/Hostx64/x64/cl.exe", missing_executable, architecture="x64")
    result = run(build, missing_executable, "MSVC")
    assert result.returncode != 0 and "does not exist" in result.stderr

    wrong_compiler = base / "wrong compiler identity"
    write_layout(wrong_compiler, "Clang", "/usr/bin/clang", launchers["linux"])
    result = run(wrong_compiler, launchers["linux"], "GNU")
    assert result.returncode != 0 and "Expected C compiler ID GNU" in result.stderr

    compiler = base / "MSVC Tools" / "Hostx64" / "x64" / "cl.exe"
    bash = base / "Git" / "bin" / "bash.exe"
    compiler.parent.mkdir(parents=True)
    bash.parent.mkdir(parents=True)
    compiler.write_text("compiler", encoding="utf-8")
    bash.write_text("bash", encoding="utf-8")
    exact = base / "exact Windows toolchain"
    write_layout(exact, "MSVC", str(compiler), launchers["windows"], architecture="x64", bash=bash)
    result = run(exact, launchers["windows"], "MSVC", "--expected-compiler", str(compiler),
                 "--expected-bash", str(bash), "--expected-architecture", "x64")
    assert result.returncode == 0, result.stderr
    wrong_bash = base / "wrong-bash.exe"
    wrong_bash.write_text("wrong", encoding="utf-8")
    result = run(exact, launchers["windows"], "MSVC", "--expected-compiler", str(compiler),
                 "--expected-bash", str(wrong_bash), "--expected-architecture", "x64")
    assert result.returncode != 0 and "BASH_EXECUTABLE: expected" in result.stderr and "actual" in result.stderr
    result = run(exact, launchers["windows"], "MSVC", "--expected-compiler", str(wrong_bash),
                 "--expected-bash", str(bash), "--expected-architecture", "x64")
    assert result.returncode != 0 and "C compiler path: expected" in result.stderr and "actual" in result.stderr

print("OK: CMake toolchain validator strictly covers Linux, macOS, Windows .exe paths, launchers, and compilers.")
