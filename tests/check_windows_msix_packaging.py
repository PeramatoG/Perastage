#!/usr/bin/env python3
"""Focused Store input contracts; Windows additionally runs SDK schema validation."""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET
import zipfile
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("perastage_msix", ROOT / "packaging/windows/msix/msix.py")
MSIX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MSIX)
NS = {**MSIX.NS, "uap": "http://schemas.microsoft.com/appx/manifest/uap/windows10",
      "uap3": "http://schemas.microsoft.com/appx/manifest/uap/windows10/3",
      "uap5": "http://schemas.microsoft.com/appx/manifest/uap/windows10/5",
      "uap10": "http://schemas.microsoft.com/appx/manifest/uap/windows10/10",
      "desktop6": "http://schemas.microsoft.com/appx/manifest/desktop/windows10/6",
      "rescap": "http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"}
# Deliberately synthetic: never use fixture identity for a Store submission.
VCLIBS_VERSION = "14.0.33519.0"  # Synthetic fixture version; production reads the SDK framework.
IDENTITY = {"Name": "Tests.Perastage", "Publisher": "CN=StorePackagingTests",
            "PublisherDisplayName": "Store Packaging Tests"}


def pe_fixture(path: Path, machine: int = 0x8664) -> None:
    header = bytearray(64)
    header[:2] = b"MZ"
    struct.pack_into("<I", header, 60, 64)
    path.write_bytes(header + b"PE\x00\x00" + struct.pack("<H", machine))


class StorePackagingTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="Store packaging spaces ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.stage = self.root / "stage"
        self.stage.mkdir()
        for name in ("Perastage.exe", "perastage-cli.exe"):
            pe_fixture(self.stage / name)
        for name in ("LICENSE.txt", "THIRD_PARTY_NOTICES.md"):
            (self.stage / name).write_text("Fixture content\n", encoding="utf-8")
        for directory in ("resources", "library"):
            (self.stage / directory).mkdir()
        (self.stage / "resources/中文 with spaces.txt").write_text("UTF-8 fixture", encoding="utf-8")
        (self.stage / "resources/literal%20 and %2520.txt").write_text("Literal percent fixture", encoding="utf-8")
        self.layout = self.root / "layout"

    def test_version_mapping_and_output_name(self) -> None:
        for version in ("1.7.0", "1.0.65535", "65535.65535.65535"):
            self.assertEqual(MSIX.package_version(version), version + ".0")
            self.assertEqual(MSIX.artifact_name(version), f"Perastage_{version}_x64.msix")
        for version in ("0.7.0", "1.65536.0", "65536.0.0", "1.0.65536", "v1.7.0", "1.7.0-rc1",
                        "1.7.0+build", "01.7.0", "1.7", "1.7.0.0", "1.7.-1", "1.7.0\n", "１.7.0"):
            with self.subTest(version=version), self.assertRaises(ValueError):
                MSIX.package_version(version)

    def test_identity_is_required_and_exact(self) -> None:
        identity_file = self.root / "identity.json"
        exact = {"Name": "Tests.Perastage.Case", "Publisher": 'CN=Fixture, O="A & B"',
                 "PublisherDisplayName": "中文 & MiXeD Publisher"}
        identity_file.write_text(json.dumps(exact), encoding="utf-8")
        self.assertEqual(MSIX.read_identity(identity_file), exact)
        tree = ET.fromstring(MSIX.manifest("1.7.0", exact, VCLIBS_VERSION))
        for key in ("Name", "Publisher"):
            self.assertEqual(tree.find("f:Identity", NS).get(key), exact[key])
        self.assertEqual(tree.findtext("f:Properties/f:PublisherDisplayName", namespaces=NS), exact["PublisherDisplayName"])

    def test_partner_center_identity_matches_supplied_values_exactly(self) -> None:
        exact = {"Name": "LuismaPeramato.Perastage", "Publisher": "CN=B827ED57-156E-455A-8B2D-C8B1FEC883AE",
                 "PublisherDisplayName": "Luisma Peramato"}
        self.assertEqual(MSIX.read_identity(None), exact)
        data = json.loads((MSIX.HERE / "identity.json").read_text())
        self.assertEqual(data["PackageFamilyName"], "LuismaPeramato.Perastage_vrw2ms6ps8f0j")
        rendered = ET.fromstring(MSIX.manifest("1.7.0", MSIX.read_identity(None), VCLIBS_VERSION))
        self.assertEqual(rendered.find("f:Identity", NS).get("Name"), exact["Name"])
        self.assertEqual(rendered.find("f:Identity", NS).get("Publisher"), exact["Publisher"])
        self.assertEqual(rendered.findtext("f:Properties/f:PublisherDisplayName", namespaces=NS), exact["PublisherDisplayName"])

    def test_missing_placeholder_and_invalid_identity_fail(self) -> None:
        identity_file = self.root / "identity.json"
        for key in MSIX.IDENTITY_KEYS:
            for value in (None, "", " ", "<PASTE EXACT PARTNER CENTER VALUE>", " CN=Fixture", "Fixture ", "Fixture\n", 12):
                values = dict(IDENTITY)
                values[key] = value
                identity_file.write_text(json.dumps(values), encoding="utf-8")
                with self.subTest(key=key, value=value), self.assertRaisesRegex(ValueError, key):
                    MSIX.read_identity(identity_file)
            identity_file.write_text(json.dumps({k: v for k, v in IDENTITY.items() if k != key}), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, key):
                MSIX.read_identity(identity_file)
        with self.assertRaisesRegex(ValueError, "placeholder"):
            MSIX.read_identity(MSIX.HERE / "identity.example.json")

    def test_manifest_desktop_associations_alias_and_virtualization(self) -> None:
        tree = ET.fromstring(MSIX.manifest("1.7.0", IDENTITY, VCLIBS_VERSION))
        self.assertEqual(tree.find("f:Identity", NS).get("Version"), "1.7.0.0")
        self.assertEqual(tree.find("f:Identity", NS).get("ProcessorArchitecture"), "x64")
        family = tree.find("f:Dependencies/f:TargetDeviceFamily", NS)
        self.assertEqual(family.get("Name"), "Windows.Desktop")
        self.assertEqual(family.get("MinVersion"), "10.0.19041.0")
        dependency = tree.find("f:Dependencies/f:PackageDependency", NS)
        self.assertEqual(dependency.get("Name"), MSIX.VCLIBS_NAME)
        self.assertEqual(dependency.get("Publisher"), MSIX.VCLIBS_PUBLISHER)
        self.assertEqual(dependency.get("MinVersion"), VCLIBS_VERSION)
        apps = tree.findall("f:Applications/f:Application", NS)
        self.assertEqual([app.get("Executable") for app in apps], ["Perastage.exe", "perastage-cli.exe"])
        for app in apps:
            self.assertEqual(app.get(f"{{{NS['uap10']}}}RuntimeBehavior"), "packagedClassicApp")
            self.assertEqual(app.get(f"{{{NS['uap10']}}}TrustLevel"), "mediumIL")
        associations = apps[0].findall("f:Extensions/uap3:Extension/uap3:FileTypeAssociation", NS)
        self.assertEqual(len(associations), 2)
        self.assertEqual([a.findtext("uap:SupportedFileTypes/uap:FileType", namespaces=NS) for a in associations], [".pstg", ".mvr"])
        for association in associations:
            self.assertEqual(association.get("Parameters"), '"%1"')
            self.assertEqual(association.get("MultiSelectModel"), "Single")
            self.assertFalse(any("TakeOver" in key for key in association.attrib))
        self.assertEqual(apps[1].get(f"{{{NS['uap10']}}}Subsystem"), "console")
        self.assertEqual(apps[1].get(f"{{{NS['uap10']}}}SupportsMultipleInstances"), "true")
        self.assertEqual(apps[1].find("uap:VisualElements", NS).get("AppListEntry"), "none")
        alias = apps[1].find("f:Extensions/uap5:Extension", NS)
        self.assertEqual(alias.get("Executable", apps[1].get("Executable")), "perastage-cli.exe")
        self.assertEqual(alias.find("uap5:AppExecutionAlias/uap5:ExecutionAlias", NS).get("Alias"), "perastage-cli.exe")
        for kind in ("FileSystem", "Registry"):
            self.assertEqual(tree.findtext(f"f:Properties/desktop6:{kind}WriteVirtualization", namespaces=NS), "disabled")
        self.assertEqual({c.get("Name") for c in tree.findall("f:Capabilities/rescap:Capability", NS)},
                         {"runFullTrust", "unvirtualizedResources"})

    def test_manifest_languages_follow_canonical_release_registry(self) -> None:
        tree = ET.fromstring(MSIX.manifest("1.7.0", IDENTITY, VCLIBS_VERSION))
        languages = [resource.get("Language") for resource in tree.findall("f:Resources/f:Resource", NS)]
        canonical = MSIX.LOCALIZATION.release_languages()
        self.assertEqual(languages, ["en-US", *(language.replace("_", "-") for language in canonical)])
        self.assertEqual(languages[0], "en-US")
        if "zh_CN" in canonical:
            self.assertIn("zh-CN", languages)
        self.assertNotIn("zh_CN", languages)

    def test_manifest_languages_track_future_registry_changes(self) -> None:
        with patch.object(MSIX, "release_languages", return_value=["es", "it", "zh_CN"]):
            tree = ET.fromstring(MSIX.manifest("1.7.0", IDENTITY, VCLIBS_VERSION))
        self.assertEqual([resource.get("Language") for resource in tree.findall("f:Resources/f:Resource", NS)],
                         ["en-US", "es", "it", "zh-CN"])

    def test_manifest_language_drift_is_rejected(self) -> None:
        template = (MSIX.HERE / "AppxManifest.xml.in").read_text(encoding="utf-8")
        languages = ["en-US", *(language.replace("_", "-") for language in MSIX.LOCALIZATION.release_languages())]
        cases = [(f"missing {language}", [item for item in languages if item != language]) for language in languages]
        cases += [("unexpected language", [*languages, "it"]),
                  ("internal Chinese locale", ["zh_CN" if item == "zh-CN" else item for item in languages]),
                  ("English no longer first", [*languages[1:], "en-US"])]
        for label, declared in cases:
            resources = "\n    ".join(f'<Resource Language="{language}" />' for language in declared)
            (self.root / "AppxManifest.xml.in").write_text(template.replace("@LANGUAGE_RESOURCES@", resources), encoding="utf-8")
            with self.subTest(case=label), patch.object(MSIX, "HERE", self.root), self.assertRaisesRegex(ValueError, "MSIX languages differ"):
                MSIX.manifest("1.7.0", IDENTITY, VCLIBS_VERSION)

    def test_framework_identity_and_version_are_read_from_retail_x64_package(self) -> None:
        framework = self.root / "framework.appx"
        def write_framework(**changes: str) -> None:
            values = {"Name": MSIX.VCLIBS_NAME, "Publisher": MSIX.VCLIBS_PUBLISHER,
                      "ProcessorArchitecture": "x64", "Version": VCLIBS_VERSION, **changes}
            package = ET.Element(f"{{{NS['f']}}}Package")
            ET.SubElement(package, f"{{{NS['f']}}}Identity", values)
            properties = ET.SubElement(package, f"{{{NS['f']}}}Properties")
            ET.SubElement(properties, f"{{{NS['f']}}}Framework").text = "true"
            with zipfile.ZipFile(framework, "w") as output:
                output.writestr("AppxManifest.xml", ET.tostring(package))
        write_framework()
        self.assertEqual(MSIX.framework_version(framework), VCLIBS_VERSION)
        for options in ({"Name": "Microsoft.VCLibs.140.00.Debug.UWPDesktop"},
                        {"Name": "Microsoft.VCLibs.140.00"}, {"ProcessorArchitecture": "x86"},
                        {"Publisher": "CN=Another Publisher"}, {"Version": "14.0.65536.0"}):
            write_framework(**options)
            with self.subTest(options=options), self.assertRaises(ValueError):
                MSIX.framework_version(framework)

    def test_prepare_is_deterministic_and_preserves_stage(self) -> None:
        before = {p.relative_to(self.stage): (p.read_bytes(), p.stat().st_mtime_ns)
                  for p in self.stage.rglob("*") if p.is_file()}
        MSIX.prepare(self.stage, self.layout, "1.7.0", IDENTITY, VCLIBS_VERSION)
        second = self.root / "second"
        MSIX.prepare(self.stage, second, "1.7.0", IDENTITY, VCLIBS_VERSION)
        for path in self.layout.rglob("*"):
            relative = path.relative_to(self.layout)
            self.assertEqual(path.stat().st_mtime, MSIX.FIXED_TIME)
            if path.is_file():
                self.assertEqual(path.read_bytes(), (second / relative).read_bytes())
        after = {p.relative_to(self.stage): (p.read_bytes(), p.stat().st_mtime_ns)
                 for p in self.stage.rglob("*") if p.is_file()}
        self.assertEqual(before, after)
        with self.assertRaisesRegex(ValueError, "must be new"):
            MSIX.prepare(self.stage, self.layout, "1.7.0", IDENTITY, VCLIBS_VERSION)

    def test_missing_binaries_and_wrong_architecture_fail(self) -> None:
        for name in ("Perastage.exe", "perastage-cli.exe"):
            binary = self.stage / name
            binary.unlink()
            with self.assertRaisesRegex(ValueError, name.replace(".", r"\.")):
                MSIX.staged_files(self.stage)
            pe_fixture(binary, 0x14c)
            with self.assertRaisesRegex(ValueError, "x64"):
                MSIX.staged_files(self.stage)
            pe_fixture(binary)

    def test_symbols_keys_and_reserved_content_fail(self) -> None:
        for name in ("private.pdb", "dev.pfx", "private.key", "AppxManifest.xml", "symbols/private.txt", "StoreAssets/extra.png"):
            path = self.stage / name
            path.parent.mkdir(exist_ok=True)
            path.write_text("Forbidden fixture")
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "Private"):
                MSIX.staged_files(self.stage)
            path.unlink()
            if path.parent != self.stage:
                path.parent.rmdir()

    def test_missing_assets_and_overlapping_layout_fail(self) -> None:
        with self.assertRaisesRegex(ValueError, "Missing MSIX visual"):
            MSIX.prepare(self.stage, self.layout, "1.7.0", IDENTITY, VCLIBS_VERSION, self.root / "missing")
        with self.assertRaisesRegex(ValueError, "outside"):
            MSIX.prepare(self.stage, self.stage / "layout", "1.7.0", IDENTITY, VCLIBS_VERSION)

    def test_archive_content_validation(self) -> None:
        MSIX.prepare(self.stage, self.layout, "1.7.0", IDENTITY, VCLIBS_VERSION)
        package = self.root / MSIX.artifact_name("1.7.0")

        def archive(extra: str | None = None, omit: str | None = None, corrupt: bool = False,
                    encoded: bool = True) -> None:
            with zipfile.ZipFile(package, "w") as output:
                for path in self.layout.rglob("*"):
                    if path.is_file():
                        name = path.relative_to(self.layout).as_posix()
                        if name != omit:
                            output.writestr(quote(name, safe="/") if encoded else name,
                                            b"altered" if corrupt and name == "Perastage.exe" else path.read_bytes())
                for name in ("AppxBlockMap.xml", "[Content_Types].xml"):
                    output.writestr(name, "fixture")
                if extra:
                    output.writestr(extra, "unexpected fixture")

        archive()
        MSIX.verify_archive(package, self.layout, "1.7.0")
        for options in ({"extra": "private.pdb"}, {"extra": "AppxSignature.p7x"}, {"omit": "perastage-cli.exe"}, {"corrupt": True}):
            archive(**options)
            with self.subTest(options=options), self.assertRaises(ValueError):
                MSIX.verify_archive(package, self.layout, "1.7.0")

    def test_store_artifact_is_outside_public_release_contract(self) -> None:
        workflow = (ROOT / ".github/workflows/windows-installer.yml").read_text()
        self.assertIn("Store-Perastage-windows-msix", workflow)
        self.assertIn("inputs.build_msix || vars.PERASTAGE_STORE_MSIX_ENABLED == 'true'", workflow)
        self.assertIn("run: ./packaging/windows/msix/build.ps1", workflow)
        self.assertIn("name: Perastage-windows-installer", workflow)
        release = (ROOT / ".github/workflows/minor-draft-release.yml").read_text()
        self.assertIn("pattern: Perastage-*", release)
        contract = json.loads((ROOT / ".github/release-artifact-contract.json").read_text())
        self.assertFalse(any("msix" in value for value in contract["artifacts"].values()))

    @unittest.skipUnless(os.name == "nt", "Windows SDK MakeAppx validation requires Windows")
    def test_windows_sdk_manifest_schema(self) -> None:
        # Parse the actual entry script without executing its release/native steps.
        parser_script = self.root / "parse-build.ps1"
        parser_script.write_text(
            "param([string]$Path)\n$tokens = $null\n$errors = $null\n"
            "[System.Management.Automation.Language.Parser]::ParseFile($Path, [ref]$tokens, [ref]$errors) | Out-Null\n"
            "if ($errors.Count) { $errors | Out-String | Write-Error; exit 1 }\n", encoding="utf-8")
        syntax = subprocess.run(["pwsh", "-NoProfile", "-File", str(parser_script), str(MSIX.HERE / "build.ps1")],
                                text=True, capture_output=True)
        self.assertEqual(syntax.returncode, 0, syntax.stdout + syntax.stderr)
        tool = shutil.which("makeappx.exe")
        if not tool:
            sdk = Path(os.environ["ProgramFiles(x86)"]) / "Windows Kits/10/bin"
            candidates = sorted(sdk.glob("10.0.*/x64/makeappx.exe"),
                                key=lambda p: tuple(map(int, p.parents[1].name.split("."))), reverse=True)
            self.assertTrue(candidates, "Windows SDK MakeAppx is required on the Windows validation runner")
            tool = str(candidates[0])
        MSIX.prepare(self.stage, self.layout, "1.7.0", IDENTITY, VCLIBS_VERSION)
        package = self.root / MSIX.artifact_name("1.7.0")
        result = subprocess.run([tool, "pack", "/d", str(self.layout), "/p", str(package), "/h", "SHA256", "/no"],
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        MSIX.verify_archive(package, self.layout, "1.7.0")


if __name__ == "__main__":
    unittest.main()
