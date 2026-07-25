#!/usr/bin/env python3
import argparse
import importlib.util
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[2] / ".github/scripts/setup_vcpkg_github_packages.py"
SPEC = importlib.util.spec_from_file_location("remote_cache", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class SetupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.env_file = Path(self.temp.name) / "env"
        self.out_file = Path(self.temp.name) / "out"
        self.environment = mock.patch.dict(os.environ, {
            "GITHUB_ENV": str(self.env_file), "GITHUB_OUTPUT": str(self.out_file),
            "RUNNER_TEMP": self.temp.name, "GITHUB_TOKEN": "secret-value"}, clear=True)
        self.environment.start()

    def tearDown(self):
        self.environment.stop()
        self.temp.cleanup()

    def args(self, mode):
        return argparse.Namespace(vcpkg="/tools/vcpkg", local_cache=self.temp.name, mode=mode)

    def test_disabled_writes_local_source_and_repository(self):
        self.assertFalse(MODULE.configure(self.args("disabled")))
        text = self.env_file.read_text()
        self.assertIn("VCPKG_BINARY_SOURCES=clear;files,", text)
        self.assertNotIn("nugetconfig", text)
        self.assertIn(MODULE.REPOSITORY_URL, text)

    @mock.patch.object(MODULE.platform, "system", return_value="Windows")
    @mock.patch.object(MODULE.subprocess, "run")
    def test_windows_read_invokes_nuget_directly(self, run, _system):
        run.return_value.stdout = "C:\\tools\\nuget.exe\n"
        self.assertTrue(MODULE.configure(self.args("read")))
        commands = [call.args[0] for call in run.call_args_list]
        self.assertTrue(any(command[0] == "C:\\tools\\nuget.exe" for command in commands))
        self.assertIn("nugetconfig", self.env_file.read_text())
        self.assertIn(",read\n", self.env_file.read_text())

    @mock.patch.object(MODULE.platform, "system", return_value="Linux")
    @mock.patch.object(MODULE.subprocess, "run")
    def test_non_windows_readwrite_uses_mono_and_api_key(self, run, _system):
        run.return_value.stdout = "/tools/nuget.exe\n"
        self.assertTrue(MODULE.configure(self.args("readwrite")))
        commands = [call.args[0] for call in run.call_args_list]
        self.assertTrue(any(command[:2] == ["mono", "/tools/nuget.exe"] for command in commands))
        self.assertTrue(any(
            "config" in command and f"defaultPushSource={MODULE.FEED_URL}" in command
            for command in commands
        ))
        self.assertTrue(any(
            "setApiKey" in command
            and command[command.index("-Source") + 1] == MODULE.FEED_URL
            for command in commands
        ))
        self.assertNotIn("secret-value", self.env_file.read_text())

    @mock.patch.object(MODULE.subprocess, "run", side_effect=OSError("offline"))
    def test_read_failure_falls_back(self, _run):
        self.assertFalse(MODULE.configure(self.args("read")))
        self.assertNotIn("nugetconfig", self.env_file.read_text())

    @mock.patch.object(MODULE.subprocess, "run")
    def test_main_writes_github_outputs_without_token(self, run):
        run.return_value.stdout = "/tools/nuget.exe\n"
        with mock.patch.object(MODULE.sys, "argv", ["setup", "--vcpkg", "/tools/vcpkg", "--local-cache", self.temp.name, "--mode", "read"]):
            self.assertEqual(MODULE.main(), 0)
        output = self.out_file.read_text()
        self.assertIn("remote-enabled=true", output)
        self.assertIn("setup-result=configured", output)
        self.assertNotIn("secret-value", output)

    @mock.patch.object(MODULE.subprocess, "run", side_effect=OSError("offline"))
    def test_writer_failure_is_fatal(self, _run):
        with self.assertRaises(RuntimeError):
            MODULE.configure(self.args("readwrite"))


if __name__ == "__main__":
    unittest.main()
