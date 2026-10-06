import os
import stat
import subprocess
import sys
from pathlib import Path

import pytest

SCRIPT = Path(__file__).resolve().parents[2] / ".github" / "scripts" / "vcpkg_install_retry.py"

# Excerpt from Weekly Compatibility Packages run 37517050256, without timestamps.
NATIVE_DOWNLOAD_TIMEOUT_OUTPUT = """Downloading gperf-3.3.tar.gz, trying https://ftpmirror.gnu.org/gnu/gperf/gperf-3.3.tar.gz
Attempt 1 of 3, retrying download.
Attempt 2 of 3, retrying download.
error: Download timed out.
error: Download timed out.
error: Reached maximum number of attempts, won't retry download from https://ftpmirror.gnu.org/gnu/gperf/gperf-3.3.tar.gz.
Trying https://ftp.gnu.org/pub/gnu/gperf/gperf-3.3.tar.gz
Attempt 1 of 3, retrying download.
Attempt 2 of 3, retrying download.
error: Download timed out.
error: Download timed out.
error: Reached maximum number of attempts, won't retry download from https://ftp.gnu.org/pub/gnu/gperf/gperf-3.3.tar.gz.
CMake Error at scripts/cmake/vcpkg_download_distfile.cmake:134 (message):
  Download failed, halting portfile.
Call Stack (most recent call first):
  scripts/cmake/vcpkg_download_distfile.cmake:156 (z_vcpkg_download_distfile)
  buildtrees/versioning_/versions/gperf/d5c53333d7745d56f06cb3e014c7c424a66ef4a7/portfile.cmake:4 (vcpkg_download_distfile)
  scripts/ports.cmake:209 (include)


error: building gperf:{triplet} failed with: BUILD_FAILED
"""


def fake_vcpkg(tmp_path: Path, body: str) -> Path:
    exe = tmp_path / ("fake vcpkg.py")
    exe.write_text(body, encoding="utf-8")
    exe.chmod(exe.stat().st_mode | stat.S_IEXEC)
    return exe


def run_wrapper(tmp_path: Path, exe: Path, attempts: int = 4) -> subprocess.CompletedProcess[str]:
    return subprocess.run([
        sys.executable, str(SCRIPT), "--vcpkg", str(exe), "--triplet", "x64-test",
        "--manifest-root", str(tmp_path / "manifest root"), "--install-root", str(tmp_path / "installed root"),
        "--packages-root", str(tmp_path / "packages root"), "--downloads-root", str(tmp_path / "downloads root"),
        "--attempts", str(attempts), "--initial-delay-seconds", "0", "--max-delay-seconds", "0",
        "--log", str(tmp_path / "logs" / "vcpkg install.log"), "--", "--debug-extra"],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def test_transient_504_twice_then_success(tmp_path):
    exe = fake_vcpkg(tmp_path, """#!/usr/bin/env python3
import pathlib, sys
count = pathlib.Path(__file__).with_suffix('.count')
n = int(count.read_text() if count.exists() else '0') + 1
count.write_text(str(n))
print('ARGS=' + repr(sys.argv[1:]))
if n < 3:
    print('error: curl operation failed with response code 504')
    sys.exit(7)
sys.exit(0)
""")
    result = run_wrapper(tmp_path, exe)
    assert result.returncode == 0
    assert result.stdout.count("vcpkg install attempt") == 3
    assert "--debug-extra" in result.stdout


def test_transient_dns_timeout_then_success(tmp_path):
    exe = fake_vcpkg(tmp_path, """#!/usr/bin/env python3
import pathlib, sys
count = pathlib.Path(__file__).with_suffix('.count')
n = int(count.read_text() if count.exists() else '0') + 1
count.write_text(str(n))
if n == 1:
    print('curl: (28) operation timed out; could not resolve host gitlab.freedesktop.org')
    sys.exit(11)
sys.exit(0)
""")
    result = run_wrapper(tmp_path, exe)
    assert result.returncode == 0
    assert result.stdout.count("vcpkg install attempt") == 2


def test_permanent_compiler_failure_no_retry(tmp_path):
    exe = fake_vcpkg(tmp_path, """#!/usr/bin/env python3
import sys
print('compilation failed: error C2143')
sys.exit(42)
""")
    result = run_wrapper(tmp_path, exe)
    assert result.returncode == 42
    assert result.stdout.count("vcpkg install attempt") == 1
    assert "failed permanently" in result.stdout


def test_final_transient_failure_preserves_exit_code(tmp_path):
    exe = fake_vcpkg(tmp_path, """#!/usr/bin/env python3
import sys
print('error: curl operation failed with response code 503')
sys.exit(9)
""")
    result = run_wrapper(tmp_path, exe, attempts=3)
    assert result.returncode == 9
    assert result.stdout.count("vcpkg install attempt") == 3
    log_path = tmp_path / "logs" / "vcpkg install.log"
    assert log_path.exists()
    assert "response code 503" in log_path.read_text(encoding="utf-8")


def test_paths_with_spaces_and_argument_forwarding(tmp_path):
    args_file = tmp_path / "args.txt"
    exe = fake_vcpkg(tmp_path, f"""#!/usr/bin/env python3
import pathlib, sys
pathlib.Path({str(args_file)!r}).write_text('\\n'.join(sys.argv[1:]), encoding='utf-8')
sys.exit(0)
""")
    result = run_wrapper(tmp_path, exe)
    assert result.returncode == 0
    args = args_file.read_text(encoding="utf-8")
    assert "install" in args
    assert f"--x-manifest-root={tmp_path / 'manifest root'}" in args
    assert "--debug-extra" in args


@pytest.mark.parametrize("triplet", ["arm64-osx", "x64-linux"])
def test_native_download_timeout_twice_then_success(tmp_path, triplet):
    output = NATIVE_DOWNLOAD_TIMEOUT_OUTPUT.format(triplet=triplet)
    exe = fake_vcpkg(tmp_path, f"""#!/usr/bin/env python3
import pathlib, sys
count = pathlib.Path(__file__).with_suffix('.count')
n = int(count.read_text() if count.exists() else '0') + 1
count.write_text(str(n))
if n < 3:
    print({output!r})
    sys.exit(1)
sys.exit(0)
""")
    result = run_wrapper(tmp_path, exe)
    assert result.returncode == 0
    assert exe.with_suffix(".count").read_text() == "3"
    assert result.stdout.count("vcpkg install attempt") == 3
    assert result.stdout.count("Transient vcpkg failure detected (error: Download timed out.)") == 2
    assert "vcpkg install succeeded on attempt 3" in result.stdout
    log = (tmp_path / "logs" / "vcpkg install.log").read_text(encoding="utf-8")
    assert log.count(output) == 2
    assert "=== attempt 3 exit code: 0 ===" in log


@pytest.mark.parametrize("triplet", ["arm64-osx", "x64-linux"])
def test_native_download_timeout_retries_are_bounded(tmp_path, triplet):
    output = NATIVE_DOWNLOAD_TIMEOUT_OUTPUT.format(triplet=triplet)
    exe = fake_vcpkg(tmp_path, f"""#!/usr/bin/env python3
import sys
print({output!r})
sys.exit(1)
""")
    result = run_wrapper(tmp_path, exe, attempts=4)
    assert result.returncode == 1
    assert result.stdout.count("vcpkg install attempt") == 4
    assert result.stdout.count("Transient vcpkg failure detected (error: Download timed out.)") == 3
    assert "vcpkg install failed after 4 transient attempts: error: Download timed out." in result.stdout
    log = (tmp_path / "logs" / "vcpkg install.log").read_text(encoding="utf-8")
    assert log.count(output) == 4
    assert "=== attempt 4 exit code: 1 ===" in log
    assert "=== vcpkg install attempt 5 ===" not in log


@pytest.mark.parametrize("output", [
    "error: building gperf:x64-linux failed with: BUILD_FAILED",
    "Download failed, halting portfile.",
    "Download failed, halting portfile.\nerror: building gperf:x64-linux failed with: BUILD_FAILED",
    "compilation failed: error C2143",
    "configuration failed: compiler is not supported",
    "configure error: missing required dependency",
    "linker error: undefined reference to main",
    "manifest validation failed: invalid dependency",
    "validation error: unsupported triplet",
    "error: SHA512 hash mismatch while downloading gperf-3.3.tar.gz",
    "error: integrity check failed for gperf-3.3.tar.gz",
    "error: gperf-3.3.tar.gz: No such file or directory",
    "error: Compilation timed out.",
    "configuration failed: Download timed out.",
])
def test_deterministic_and_generic_download_failures_no_retry(tmp_path, output):
    exe = fake_vcpkg(tmp_path, f"""#!/usr/bin/env python3
import sys
print({output!r})
sys.exit(42)
""")
    result = run_wrapper(tmp_path, exe)
    assert result.returncode == 42
    assert result.stdout.count("vcpkg install attempt") == 1
    assert "failed permanently" in result.stdout
    assert "Transient vcpkg failure detected" not in result.stdout
