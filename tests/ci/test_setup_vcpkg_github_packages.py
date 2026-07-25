#!/usr/bin/env python3
import argparse
import importlib.util
import os
import tempfile
import unittest
import xml.etree.ElementTree as ET
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
        self.mock_mode = "read"

    def tearDown(self):
        self.environment.stop()
        self.temp.cleanup()

    def args(self, mode):
        self.mock_mode = mode
        return argparse.Namespace(vcpkg="/tools/vcpkg", local_cache=self.temp.name, mode=mode)

    def successful_run(self, command, **_kwargs):
        stdout = "/tools/nuget.exe\n" if "fetch" in command else ""
        if "sources" in command and "Add" in command:
            config = Path(command[command.index("-ConfigFile") + 1])
            self.write_config(config, readwrite=self.mock_mode == "readwrite")
        return mock.Mock(stdout=stdout, stderr="", returncode=0)

    def write_config(self, path, *, readwrite=False, source=True, feed_url=None,
                     credentials=True, password=True, default_push=True, api_key=True):
        root = ET.Element("configuration")
        sources = ET.SubElement(root, "packageSources")
        if source:
            ET.SubElement(sources, "add", key=MODULE.SOURCE_NAME,
                          value=feed_url or MODULE.FEED_URL)
        credential_root = ET.SubElement(root, "packageSourceCredentials")
        if credentials:
            section = ET.SubElement(credential_root, MODULE.SOURCE_NAME)
            ET.SubElement(section, "add", key="Username", value="PeramatoG")
            if password:
                ET.SubElement(section, "add", key="ClearTextPassword", value="secret-value")
        if readwrite:
            config = ET.SubElement(root, "config")
            if default_push:
                ET.SubElement(config, "add", key="defaultPushSource", value=MODULE.FEED_URL)
            api_keys = ET.SubElement(root, "apikeys")
            if api_key:
                ET.SubElement(api_keys, "add", key=MODULE.FEED_URL, value="secret-value")
        ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)

    def test_disabled_writes_local_source_and_repository(self):
        self.assertFalse(MODULE.configure(self.args("disabled")))
        text = self.env_file.read_text()
        self.assertIn("VCPKG_BINARY_SOURCES=clear;files,", text)
        self.assertNotIn("nugetconfig", text)
        self.assertIn(MODULE.REPOSITORY_URL, text)

    @mock.patch.object(MODULE.platform, "system", return_value="Windows")
    @mock.patch.object(MODULE.subprocess, "run")
    def test_windows_read_invokes_nuget_directly(self, run, _system):
        def windows_run(command, **kwargs):
            result = self.successful_run(command, **kwargs)
            if "fetch" in command:
                result.stdout = "C:\\tools\\nuget.exe\n"
            return result

        run.side_effect = windows_run
        self.assertTrue(MODULE.configure(self.args("read")))
        commands = [call.args[0] for call in run.call_args_list]
        self.assertTrue(any(command[0] == "C:\\tools\\nuget.exe" for command in commands))
        self.assertFalse(any("list" in command for command in commands))
        self.assertFalse(any("List" in command for command in commands))
        self.assertIn("nugetconfig", self.env_file.read_text())
        self.assertIn(",read\n", self.env_file.read_text())

    @mock.patch.object(MODULE.platform, "system", return_value="Linux")
    @mock.patch.object(MODULE.subprocess, "run")
    def test_non_windows_readwrite_uses_mono_and_api_key(self, run, _system):
        run.side_effect = self.successful_run
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

    def test_valid_read_configuration(self):
        path = Path(self.temp.name) / "read.config"
        self.write_config(path)
        MODULE.validate_config(path, "read")

    def test_valid_readwrite_configuration(self):
        path = Path(self.temp.name) / "readwrite.config"
        self.write_config(path, readwrite=True)
        MODULE.validate_config(path, "readwrite")

    def assert_validation_error(self, message, **config_options):
        path = Path(self.temp.name) / "invalid.config"
        self.write_config(path, **config_options)
        with self.assertRaisesRegex(RuntimeError, message):
            MODULE.validate_config(path, "readwrite" if config_options.get("readwrite") else "read")

    def test_missing_source(self):
        self.assert_validation_error("missing package source", source=False)

    def test_incorrect_feed_url(self):
        self.assert_validation_error("unexpected feed URL", feed_url="https://example.invalid")

    def test_missing_credential_section(self):
        self.assert_validation_error("missing credential section", credentials=False)

    def test_missing_password_entry(self):
        self.assert_validation_error("missing password entry", password=False)

    def test_missing_default_push_source(self):
        self.assert_validation_error("missing default push source", readwrite=True,
                                     default_push=False)

    def test_missing_api_key_entry(self):
        self.assert_validation_error("missing API-key entry", readwrite=True, api_key=False)

    @mock.patch.object(MODULE.subprocess, "run")
    def test_validation_does_not_require_sources_list_stdout(self, run):
        def run_without_listing(command, **kwargs):
            result = self.successful_run(command, **kwargs)
            if "fetch" not in command:
                result.stdout = "output without source metadata\n"
            return result

        run.side_effect = run_without_listing
        self.assertTrue(MODULE.configure(self.args("read")))
        self.assertFalse(any("List" in call.args[0] for call in run.call_args_list))

    def test_redaction_removes_tokens_and_credential_xml_values(self):
        diagnostic = (
            'secret-value -Password another-secret setApiKey third-secret '
            '<add key="ClearTextPassword" value="xml-secret" />'
        )
        redacted = MODULE.redact_diagnostics(diagnostic, ("secret-value",))
        for secret in ("secret-value", "another-secret", "third-secret", "xml-secret"):
            self.assertNotIn(secret, redacted)
        self.assertIn("[REDACTED]", redacted)

    @mock.patch.object(MODULE.subprocess, "run")
    def test_failure_diagnostic_identifies_stage_and_redacts_token(self, run):
        run.side_effect = [
            mock.Mock(stdout="/tools/nuget.exe\n", stderr="", returncode=0),
            MODULE.subprocess.CalledProcessError(
                17, ["nuget"], output="token secret-value", stderr="password secret-value"
            ),
        ]
        with self.assertRaisesRegex(RuntimeError, "add-source.*exit code: 17") as raised:
            MODULE.configure(self.args("readwrite"))
        self.assertNotIn("secret-value", str(raised.exception))

    @mock.patch.object(MODULE.subprocess, "run", side_effect=OSError("offline"))
    def test_read_failure_falls_back(self, _run):
        self.assertFalse(MODULE.configure(self.args("read")))
        self.assertNotIn("nugetconfig", self.env_file.read_text())

    @mock.patch.object(MODULE.subprocess, "run")
    def test_read_validation_failure_falls_back(self, run):
        def invalid_config_run(command, **_kwargs):
            if "fetch" in command:
                return mock.Mock(stdout="/tools/nuget.exe\n", stderr="", returncode=0)
            if "sources" in command:
                config = Path(command[command.index("-ConfigFile") + 1])
                self.write_config(config, source=False)
            return mock.Mock(stdout="", stderr="", returncode=0)

        run.side_effect = invalid_config_run
        self.assertFalse(MODULE.configure(self.args("read")))
        self.assertNotIn("nugetconfig", self.env_file.read_text())

    @mock.patch.object(MODULE.subprocess, "run")
    def test_main_writes_github_outputs_without_token(self, run):
        run.side_effect = self.successful_run
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

    @mock.patch.object(MODULE.subprocess, "run")
    def test_writer_validation_failure_is_fatal(self, run):
        def invalid_config_run(command, **_kwargs):
            if "fetch" in command:
                return mock.Mock(stdout="/tools/nuget.exe\n", stderr="", returncode=0)
            if "sources" in command:
                config = Path(command[command.index("-ConfigFile") + 1])
                self.write_config(config, readwrite=True, api_key=False)
            return mock.Mock(stdout="", stderr="", returncode=0)

        run.side_effect = invalid_config_run
        with self.assertRaisesRegex(RuntimeError, "missing API-key entry"):
            MODULE.configure(self.args("readwrite"))


if __name__ == "__main__":
    unittest.main()
