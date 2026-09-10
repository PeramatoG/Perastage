/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/zipstrm.h>

#include "mvrimporter.h"

// Builds an in-memory MVR archive with entries in deterministic order.
static std::vector<std::uint8_t> BuildArchive(
    const std::vector<std::pair<std::string, std::string>> &entries) {
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
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"5\" provider=\"Legacy\" providerVersion=\"1\">"
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
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" provider=\"Test\" providerVersion=\"1\">"
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
  else
    assert(false);
  return 0;
}
