#!/usr/bin/env python3

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
CMAKE_LISTS = ROOT / "tests" / "CMakeLists.txt"
EXPECTED_TESTS = {
    "PowerShellNativeCaptureWindowsPowerShell": "PERASTAGE_POWERSHELL_51",
    "PowerShellNativeCapturePowerShell7": "PERASTAGE_POWERSHELL_7",
}


def main() -> int:
    lines = CMAKE_LISTS.read_text(encoding="utf-8").splitlines()
    condition_stack: list[str] = []
    found_tests: set[str] = set()

    for line_number, line in enumerate(lines, start=1):
        stripped = line.strip()
        if_match = re.fullmatch(r"if\((.+)\)", stripped, re.IGNORECASE)
        if if_match:
            condition_stack.append(if_match.group(1).strip())

        for test_name, executable_variable in EXPECTED_TESTS.items():
            if test_name not in stripped:
                continue
            if "WIN32" not in condition_stack:
                raise AssertionError(
                    f"{test_name} must be registered inside an if(WIN32) host guard "
                    f"({CMAKE_LISTS}:{line_number})."
                )
            if executable_variable not in "\n".join(lines[max(0, line_number - 4):line_number + 2]):
                raise AssertionError(
                    f"{test_name} must retain its {executable_variable} executable selection."
                )
            found_tests.add(test_name)

        if re.fullmatch(r"endif\(.*\)", stripped, re.IGNORECASE):
            if condition_stack:
                condition_stack.pop()

    missing_tests = sorted(EXPECTED_TESTS.keys() - found_tests)
    if missing_tests:
        raise AssertionError(
            "Missing Windows PowerShell native-capture registrations: "
            + ", ".join(missing_tests)
        )

    print("Windows PowerShell test registration checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
