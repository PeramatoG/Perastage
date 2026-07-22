#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKED_ROOTS = [ROOT / "tests"]
IGNORED_DIRS = {".git", "build", "out", "third_party", "vcpkg", ".vcpkg-cache"}
ALLOWED_FILES = {
    Path("tests/check_unresolved_python_invocations.py"),
    Path("tests/ci/test_vcpkg_install_retry.py"),
}
DIRECT_PYTHON = re.compile(
    r"(?m)(?:^|[;&|`$()]\s*)(?P<cmd>python3?|/[^\s'\"]*/python3?)(?:\s|$)"
)
ALLOWED_CONTEXTS = (
    "#!/usr/bin/env python3",
    "PERASTAGE_TEST_PYTHON",
    "run_test_python",
    "resolve_test_python",
    "Get-Command python",
)


def is_ignored(path: Path) -> bool:
    rel = path.relative_to(ROOT)
    return rel in ALLOWED_FILES or any(part in IGNORED_DIRS for part in rel.parts)


def main() -> int:
    violations: list[str] = []
    for root in CHECKED_ROOTS:
        for path in root.rglob("*"):
            if not path.is_file() or is_ignored(path):
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                text = path.read_text(errors="ignore")
            for line_number, line in enumerate(text.splitlines(), start=1):
                if not DIRECT_PYTHON.search(line):
                    continue
                if any(marker in line for marker in ALLOWED_CONTEXTS):
                    continue
                violations.append(f"{path.relative_to(ROOT)}:{line_number}: {line.strip()}")
    if violations:
        print("Portable tests must use PERASTAGE_TEST_PYTHON/run_test_python instead of unresolved python aliases:")
        print("\n".join(violations))
        return 1
    print("OK: portable tests do not invoke unresolved python aliases.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
