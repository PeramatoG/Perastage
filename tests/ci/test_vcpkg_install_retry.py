import os
import stat
import subprocess
import sys
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / ".github" / "scripts" / "vcpkg_install_retry.py"


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
