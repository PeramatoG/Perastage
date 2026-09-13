/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "fixture_label_overrides.h"
#include "mvr_import_package.h"
#include "mvr_import_project_application.h"
#include "mvrimporter.h"

// Builds an in-memory MVR archive with entries in deterministic order.
static std::vector<std::uint8_t>
BuildArchive(const std::vector<std::pair<std::string, std::string>> &entries) {
  wxMemoryOutputStream memory;
  {
    wxZipOutputStream zip(memory);
    for (const auto &[name, payload] : entries) {
      assert(zip.PutNextEntry(name));
      zip.Write(payload.data(), payload.size());
    }
    zip.Close();
  }
  std::vector<std::uint8_t> bytes(memory.GetSize());
  memory.CopyTo(bytes.data(), bytes.size());
  return bytes;
}

// Imports bytes through the isolated parse-only seam.
static bool Import(const std::vector<std::uint8_t> &bytes,
                   MvrImportResult &result) {
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.allowDummyFallback = false;
  MvrImporter importer;
  return importer.ImportFromBuffer(bytes, result, MvrImportMode::ParseOnly,
                                   options);
}

// Verifies mandatory root scene-description failures remain deterministic.
static void TestStrictArchiveRejection() {
  MvrImportResult result;
  assert(!Import(BuildArchive({{"resource.txt", "payload"}}), result));
  assert(!Import(BuildArchive({{"GeneralSceneDescription.xml", "<broken"}}),
                 result));
}

// Verifies the documented legacy case-insensitive root-name compatibility path.
static void TestLegacyRootNameCompatibility() {
  const std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"5\" "
      "provider=\"Legacy\" providerVersion=\"1\">"
      "<Scene><Layers/></Scene></GeneralSceneDescription>";
  MvrImportResult result;
  assert(Import(BuildArchive({{"generalscenedescription.xml", xml}}), result));
  assert(result.scene.versionMajor == 1);
  assert(result.scene.versionMinor == 5);
  assert(result.scene.provider == "Legacy");
}

// Verifies recoverable extension defects produce stable structured diagnostics.
static void TestRecoverableUserDataDiagnostics() {
  const std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Test\" providerVersion=\"1\">"
      "<UserData><Unknown/><Data ver=\"1\"><Value/></Data></UserData>"
      "<UserData><Data provider=\"Ignored\"/></UserData>"
      "<Scene><Layers/></Scene></GeneralSceneDescription>";
  MvrImportResult result;
  assert(Import(BuildArchive({{"GeneralSceneDescription.xml", xml}}), result));
  const auto hasCode = [&](const std::string &code) {
    for (const MvrImportDiagnostic &diagnostic : result.diagnostics) {
      if (diagnostic.code == code)
        return true;
    }
    return false;
  };
  assert(hasCode("multiple_root_userdata"));
  assert(hasCode("invalid_root_userdata_child"));
  assert(hasCode("missing_userdata_provider"));
}

// Verifies package acquisition safety, diagnostics, discovery, and lifetime.
static void TestPackageAcquisitionBoundary() {
  const std::string xml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\">"
      "<Scene><Layers/></Scene></GeneralSceneDescription>";
  const std::vector<std::uint8_t> validBytes = BuildArchive(
      {{"../outside.txt", "unsafe"}, {"generalscenedescription.xml", xml}});
  wxMemoryInputStream validInput(validBytes.data(), validBytes.size());
  std::vector<MvrImportDiagnostic> diagnostics;
  std::optional<mvr::ImportPackage> package =
      mvr::AcquireImportPackage(validInput, diagnostics);
  assert(package);
  assert(std::filesystem::exists(package->sceneXmlPath));
  assert(package->sceneXmlPath.parent_path() == package->rootPath);
  assert(!std::filesystem::exists(package->rootPath.parent_path() /
                                  "outside.txt"));

  const std::vector<std::uint8_t> collisionBytes =
      BuildArchive({{"GeneralSceneDescription.xml", xml},
                    {"generalscenedescription.xml", xml}});
  wxMemoryInputStream collisionInput(collisionBytes.data(),
                                     collisionBytes.size());
  diagnostics.clear();
  assert(!mvr::AcquireImportPackage(collisionInput, diagnostics));
  assert(diagnostics.size() == 1);
  assert(diagnostics.front().code == "case_colliding_mvr_archive_entry");

  const std::vector<std::uint8_t> invalidBytes = {'n', 'o', 't', 'z', 'i', 'p'};
  wxMemoryInputStream invalidInput(invalidBytes.data(), invalidBytes.size());
  diagnostics.clear();
  assert(!mvr::AcquireImportPackage(invalidInput, diagnostics));
}

// Verifies parse-only and failed imports cannot mutate the active project.
static void TestParseOnlyAndFailureAtomicity() {
  ConfigManager &config = ConfigManager::Get();
  config.Reset();
  config.GetScene().provider = "Sentinel provider";
  config.SetValue("mvr_application_sentinel", "preserved");
  viewer2d::FixtureLabelOverride overrideValue;
  overrideValue.showLabelName[0] = true;
  viewer2d::SaveFixtureLabelOverrides(config, {{"old-fixture", overrideValue}});

  const std::string xml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Imported provider\"><Scene><Layers><Layer "
      "uuid=\"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\" name=\"Layer\">"
      "<ChildList><Fixture "
      "uuid=\"{33333333-3333-4333-8333-333333333333}\" name=\"Fixture\">"
      "<Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
      "<FixtureID>1</FixtureID><FixtureIDNumeric>1</FixtureIDNumeric>"
      "</Fixture></ChildList></Layer></Layers></Scene>"
      "<UserData><Unknown/></UserData>"
      "</GeneralSceneDescription>";
  const std::vector<std::uint8_t> bytes =
      BuildArchive({{"GeneralSceneDescription.xml", xml}});
  MvrImportResult result;
  assert(Import(bytes, result));
  assert(result.scene.provider == "Imported provider");
  assert(result.scene.fixtures.size() == 1);
  assert(result.fixtureUuidRemap.size() == 1);
  assert(!result.diagnostics.empty());
  assert(config.GetScene().provider == "Sentinel provider");
  assert(config.GetValue("mvr_application_sentinel") == "preserved");
  assert(viewer2d::LoadFixtureLabelOverrides(config).contains("old-fixture"));

  assert(!Import(BuildArchive({{"GeneralSceneDescription.xml", "<broken"}}),
                 result));
  assert(config.GetScene().provider == "Sentinel provider");
  assert(config.GetValue("mvr_application_sentinel") == "preserved");
  assert(viewer2d::LoadFixtureLabelOverrides(config).contains("old-fixture"));

  MvrImportOptions replaceOptions;
  replaceOptions.promptConflicts = false;
  replaceOptions.applyDictionary = false;
  replaceOptions.allowDummyFallback = false;
  MvrImporter importer;
  assert(importer.ImportFromBuffer(bytes, result, MvrImportMode::ReplaceProject,
                                   replaceOptions));
  assert(result.scene.provider == "Imported provider");
  assert(result.scene.fixtures.size() == 1);
  assert(result.fixtureUuidRemap.size() == 1);
  assert(!result.diagnostics.empty());
  assert(config.GetScene().provider == "Imported provider");
}

// Verifies replacement reset, resource lifetime, and result preservation.
static void TestProjectApplicationBoundary() {
  ConfigManager &config = ConfigManager::Get();
  config.Reset();
  config.SetValue("mvr_application_sentinel", "cleared by reset");
  viewer2d::FixtureLabelOverride oldOverride;
  oldOverride.showLabelId[1] = true;
  viewer2d::FixtureLabelOverride collisionOverride;
  collisionOverride.showLabelDmx[2] = true;
  viewer2d::FixtureLabelOverride unrelatedOverride;
  unrelatedOverride.labelFontSizeName = 14.0F;
  viewer2d::SaveFixtureLabelOverrides(
      config, {{"old-fixture", oldOverride},
               {"collision-target", collisionOverride},
               {"unrelated-fixture", unrelatedOverride}});

  MvrImportResult result;
  result.scene.provider = "Applied provider";
  const auto lease = std::make_shared<int>(42);
  result.scene.runtimeResourceLeases.push_back(lease);
  result.fixtureUuidRemap = {{"old-fixture", "new-fixture"},
                             {"collision-source", "collision-target"},
                             {"unrelated-fixture", "unrelated-fixture"}};
  viewer2d::FixtureLabelOverride collisionSource;
  collisionSource.showLabelName[0] = false;
  auto overrides = viewer2d::LoadFixtureLabelOverrides(config);
  overrides.emplace("collision-source", collisionSource);
  viewer2d::SaveFixtureLabelOverrides(config, overrides);

  mvr::MvrImportProjectApplication application(config);
  const mvr::ProjectApplicationResult applied = application.Apply(result);
  assert(config.GetScene().provider == "Applied provider");
  assert(!config.GetValue("mvr_application_sentinel"));
  assert(config.GetScene().runtimeResourceLeases.size() == 1);
  assert(config.GetScene().runtimeResourceLeases.front() == lease);
  assert(applied.migratedFixtureLabelOverrides == 0);
  assert(applied.fixtureLabelOverrideCollisions == 0);
  const auto migrated = viewer2d::LoadFixtureLabelOverrides(config);
  assert(migrated.empty());
  assert(result.scene.provider == "Applied provider");
  assert(result.scene.runtimeResourceLeases.front() == lease);
  assert(result.fixtureUuidRemap.size() == 3);

  MvrImportResult emptyRemapResult;
  emptyRemapResult.scene.provider = "Empty remap provider";
  const mvr::ProjectApplicationResult emptyApplied =
      application.Apply(emptyRemapResult);
  assert(config.GetScene().provider == "Empty remap provider");
  assert(emptyApplied.migratedFixtureLabelOverrides == 0);
  assert(emptyApplied.fixtureLabelOverrideCollisions == 0);
}

// Verifies external replacement clears stale overrides despite UUID remapping.
static void TestExternalImportResetSemantics() {
  ConfigManager &config = ConfigManager::Get();
  config.Reset();
  config.SetValue("mvr_application_sentinel", "cleared by reset");
  viewer2d::FixtureLabelOverride staleOverride;
  staleOverride.showLabelName[0] = true;
  viewer2d::SaveFixtureLabelOverrides(config, {{"old-fixture", staleOverride}});

  MvrImportResult result;
  result.scene.provider = "External provider";
  result.fixtureUuidRemap = {{"old-fixture", "new-fixture"}};
  result.diagnostics.push_back({"preserved_diagnostic", "Still available"});

  mvr::MvrImportProjectApplication application(config);
  application.Apply(result);
  assert(config.GetScene().provider == "External provider");
  assert(!config.GetValue("mvr_application_sentinel"));
  assert(viewer2d::LoadFixtureLabelOverrides(config).empty());
  assert(result.scene.provider == "External provider");
  assert(result.fixtureUuidRemap.at("old-fixture") == "new-fixture");
  assert(result.diagnostics.front().code == "preserved_diagnostic");
}

// Verifies file and buffer registration share equivalent project application.
static void TestFileAndBufferRegistrationParity() {
  const std::string xml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Parity provider\"><Scene><Layers/></Scene>"
      "</GeneralSceneDescription>";
  const std::vector<std::uint8_t> bytes =
      BuildArchive({{"GeneralSceneDescription.xml", xml}});
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.allowDummyFallback = false;

  ConfigManager &config = ConfigManager::Get();
  assert(MvrImporter::ImportAndRegisterFromBuffer(bytes, options));
  assert(config.GetScene().provider == "Parity provider");
  const size_t bufferLeaseCount =
      config.GetScene().runtimeResourceLeases.size();

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "perastage-ch105-parity.mvr";
  {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  }
  assert(MvrImporter::ImportAndRegister(path.string(), options));
  std::filesystem::remove(path);
  assert(config.GetScene().provider == "Parity provider");
  assert(config.GetScene().runtimeResourceLeases.size() == bufferLeaseCount);
}

// Runs one independently labeled importer characterization scenario.
int main(int argc, char **argv) {
  wxInitializer initializer;
  assert(initializer.IsOk());
  assert(argc == 2);
  const std::string scenario = argv[1];
  if (scenario == "strict")
    TestStrictArchiveRejection();
  else if (scenario == "legacy")
    TestLegacyRootNameCompatibility();
  else if (scenario == "recovery")
    TestRecoverableUserDataDiagnostics();
  else if (scenario == "package")
    TestPackageAcquisitionBoundary();
  else if (scenario == "application") {
    TestParseOnlyAndFailureAtomicity();
    TestExternalImportResetSemantics();
    TestProjectApplicationBoundary();
    TestFileAndBufferRegistrationParity();
  } else
    assert(false);
  return 0;
}
