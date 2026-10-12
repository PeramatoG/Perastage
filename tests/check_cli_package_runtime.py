#!/usr/bin/env python3
"""Exercise the production AppRun argv dispatch and macOS dependency validation."""

from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "macos_dependencies", ROOT / ".github/scripts/check_macos_bundle_dependencies.py")
MACOS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MACOS)


class AppRunTests(unittest.TestCase):
    """Run the workflow's actual launcher with recording stand-ins for both executables."""

    @unittest.skipIf(sys.platform == "win32" or shutil.which("bash") is None,
                     "AppImage launcher execution requires a POSIX host with Bash")
    def test_dispatch_preserves_arguments_and_gui_behavior(self) -> None:
        """Only the first exact --cli switches executables; shell syntax stays literal."""
        workflow = (ROOT / ".github/workflows/linux-installer.yml").read_text()
        block = re.search(r"printf '%s\\n' \\\n\s*'#!/usr/bin/env bash'.*?> \"\$APPDIR/AppRun\"",
                          workflow, re.DOTALL).group()
        launcher = "\n".join(re.findall(r"^\s*'([^']*)'", block, re.MULTILINE)) + "\n"
        with tempfile.TemporaryDirectory(prefix="AppDir with spaces ") as directory:
            appdir = Path(directory)
            binaries = appdir / "usr/bin"
            binaries.mkdir(parents=True)
            (appdir / "AppRun").write_text(launcher)
            for name in ("Perastage", "perastage-cli"):
                binary = binaries / name
                binary.write_text("#!/usr/bin/env python3\nimport json, os, sys\n"
                                  "print(json.dumps([os.path.basename(sys.argv[0]), "
                                  "sys.argv[1:], os.getcwd()]))\n")
                binary.chmod(0o755)
            cases = (
                ([], "Perastage", [], str(binaries.resolve())),
                (["--cli"], "perastage-cli", [], os.getcwd()),
                (["--cli", "--version"], "perastage-cli", ["--version"], os.getcwd()),
                (["--cli", "scene", "路径 with spaces.mvr", "", "$(touch injected)", "--cli"],
                 "perastage-cli", ["scene", "路径 with spaces.mvr", "", "$(touch injected)", "--cli"], os.getcwd()),
                (["file.mvr", "--cli"], "Perastage", ["file.mvr", "--cli"], str(binaries.resolve())),
                (["--client"], "Perastage", ["--client"], str(binaries.resolve())),
            )
            for arguments, name, expected, cwd in cases:
                with self.subTest(arguments=arguments):
                    result = subprocess.check_output(["bash", str(appdir / "AppRun"), *arguments], text=True)
                    self.assertEqual(json.loads(result), [name, expected, cwd])


class MacosDependencyTests(unittest.TestCase):
    """Require actual packaged files for non-system load paths."""

    def test_cli_specific_missing_dependency_fails_bundle_audit(self) -> None:
        """A working GUI must not hide a missing dylib used only by the CLI."""
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory) / "Perastage.app"
            binaries = bundle / "Contents/MacOS"
            binaries.mkdir(parents=True)
            for name in ("Perastage", "perastage-cli"):
                (binaries / name).touch()

            def inspect(*arguments: str) -> str:
                """Supply native-tool facts for a GUI and a CLI with distinct dependencies."""
                if arguments[0] == "file":
                    return "Mach-O 64-bit executable arm64"
                if arguments[1] == "-l":
                    return ""
                dependency = ("@loader_path/../Frameworks/libCliOnly.dylib"
                              if Path(arguments[2]).name == "perastage-cli"
                              else "/usr/lib/libSystem.B.dylib")
                return arguments[2] + ":\n\t" + dependency + " (compatibility version 1.0.0)\n"

            with patch.object(MACOS, "output", side_effect=inspect):
                with self.assertRaisesRegex(RuntimeError, "libCliOnly"):
                    MACOS.check(bundle)
                library = bundle / "Contents/Frameworks/libCliOnly.dylib"
                library.parent.mkdir()
                library.touch()
                MACOS.check(bundle)

    def test_relative_and_rpath_resolution(self) -> None:
        """Resolve shared dylibs from the CLI; reject missing, external and escaping paths."""
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory) / "Perastage with spaces.app"
            executable = bundle / "Contents/MacOS/perastage-cli"
            library = bundle / "Contents/Frameworks/libExample.dylib"
            library.parent.mkdir(parents=True)
            library.touch()
            for dependency in ("@executable_path/../Frameworks/libExample.dylib",
                               "@loader_path/../Frameworks/libExample.dylib",
                               "@rpath/libExample.dylib", "/usr/lib/libSystem.B.dylib"):
                MACOS.check_dependency(dependency, executable, executable, bundle,
                                       ["@executable_path/../Frameworks"])
            outside = Path(directory) / "libOutside.dylib"
            outside.touch()
            for dependency in ("@loader_path/../Frameworks/missing.dylib",
                               "@rpath/missing.dylib", str(outside),
                               "@executable_path/../../../libOutside.dylib"):
                with self.subTest(dependency=dependency), self.assertRaises(RuntimeError):
                    MACOS.check_dependency(dependency, executable, executable, bundle,
                                           ["@executable_path/../Frameworks"])


if __name__ == "__main__":
    unittest.main()
