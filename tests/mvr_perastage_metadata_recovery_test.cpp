/*
 * This file is part of Perastage.
 */
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "mvrimporter.h"

namespace fs = std::filesystem;

// Writes ordered ZIP entries without deduplicating their archive names.
static void WriteArchive(
    const fs::path &path,
    const std::vector<std::pair<std::string, std::string>> &entries) {
  wxFileOutputStream output(path.generic_string());
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  for (const auto &[name, bytes] : entries) {
    assert(zip.PutNextEntry(name));
    zip.Write(bytes.data(), bytes.size());
  }
  zip.Close();
}

// Builds a minimal MVR scene whose TrussInfo references the requested resource.
static std::string BuildAuxiliarySceneXml(const std::string &auxiliaryName) {
  return "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" provider=\"Test\" providerVersion=\"1\">"
         "<UserData><Data provider=\"Perastage\" ver=\"1.0\"><TrussInfoMap>"
         "<TrussInfo uuid=\"40000000-0000-4000-8000-000000000001\"><Manufacturer>Unrelated</Manufacturer><AuxGdtf>" +
         auxiliaryName +
         "</AuxGdtf></TrussInfo></TrussInfoMap></Data></UserData>"
         "<Scene><Layers><Layer uuid=\"10000000-0000-4000-8000-000000000001\" name=\"Default\"><ChildList>"
         "<Truss uuid=\"40000000-0000-4000-8000-000000000001\" name=\"Truss\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix><Geometries/><FixtureID>1</FixtureID><FixtureIDNumeric>1</FixtureIDNumeric></Truss>"
         "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
}

// Writes one deterministic minimal MVR archive for metadata recovery testing.
static void WriteMetadataArchive(const fs::path &path) {
  static const std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" provider=\"Test\" providerVersion=\"1\">"
      "<UserData>"
      "<Data provider=\"Perastage\" ver=\"1.0\"><HoistInfoMap>"
      "<HoistInfo uuid=\"30000000-0000-4000-8000-000000000001\"><Capacity>100</Capacity><Weight>NaN</Weight><MotorFixtureUuid>20000000-0000-4000-8000-000000000001</MotorFixtureUuid><UseMotorDefaults>maybe</UseMotorDefaults><Unknown/></HoistInfo>"
      "<HoistInfo uuid=\"30000000-0000-4000-8000-000000000003\"><Capacity>55</Capacity><MotorFixtureUuid>20000000-0000-4000-8000-000000000099</MotorFixtureUuid></HoistInfo>"
      "<HoistInfo uuid=\"{30000000-0000-4000-8000-000000000001}\"><Capacity>999</Capacity></HoistInfo>"
      "<HoistInfo uuid=\"bad\"><Capacity>888</Capacity></HoistInfo>"
      "<HoistInfo uuid=\"30000000-0000-4000-8000-000000000099\"><Capacity>777</Capacity></HoistInfo>"
      "</HoistInfoMap><TrussInfoMap>"
      "<TrussInfo uuid=\"40000000-0000-4000-8000-000000000001\"><Length>infinity</Length><Width>250</Width><AuxGdtf>../escape.gdtf</AuxGdtf></TrussInfo>"
      "<TrussInfo uuid=\"40000000-0000-4000-8000-000000000001\"><Width>999</Width></TrussInfo>"
      "<TrussInfo uuid=\"bad\"/><TrussInfo uuid=\"40000000-0000-4000-8000-000000000099\"/>"
      "</TrussInfoMap></Data>"
      "<Data provider=\"Perastage\" ver=\"2.0\"><HoistInfoMap><HoistInfo uuid=\"30000000-0000-4000-8000-000000000001\"><Capacity>555</Capacity></HoistInfo></HoistInfoMap></Data>"
      "<Data provider=\"Foreign\" ver=\"1.0\"><HoistInfoMap><HoistInfo uuid=\"30000000-0000-4000-8000-000000000001\"><Capacity>444</Capacity></HoistInfo></HoistInfoMap></Data>"
      "</UserData><Scene><Layers><Layer uuid=\"10000000-0000-4000-8000-000000000001\" name=\"Default\"><ChildList>"
      "<Fixture uuid=\"20000000-0000-4000-8000-000000000001\" name=\"Motor\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix><FixtureID>1</FixtureID><FixtureIDNumeric>1</FixtureIDNumeric></Fixture>"
      "<Support uuid=\"30000000-0000-4000-8000-000000000001\" name=\"Target\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix><Geometries/><Function>Other</Function><ChainLength>1</ChainLength><UserData><Data provider=\"Foreign\" ver=\"1.0\"><HoistInfo><Capacity>333</Capacity></HoistInfo></Data><Data provider=\"Perastage\" ver=\"1.0\"><HoistInfo><Capacity>222</Capacity></HoistInfo></Data></UserData></Support>"
      "<Support uuid=\"30000000-0000-4000-8000-000000000002\" name=\"Legacy\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix><Geometries/><ChainLength>1</ChainLength><UserData><Data provider=\"Perastage\"><MotorInfo><Capacity>42</Capacity><MotorFixtureUuid>bad</MotorFixtureUuid></MotorInfo></Data></UserData></Support>"
      "<Support uuid=\"30000000-0000-4000-8000-000000000003\" name=\"Unknown Motor\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix><Geometries/><ChainLength>1</ChainLength></Support>"
      "<Truss uuid=\"40000000-0000-4000-8000-000000000001\" name=\"Truss\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix><Geometries/><FixtureID>2</FixtureID><FixtureIDNumeric>2</FixtureIDNumeric><UserData><Data provider=\"Foreign\" ver=\"1.0\"><TrussInfo><Width>777</Width></TrussInfo></Data></UserData></Truss>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  WriteArchive(path, {{"GeneralSceneDescription.xml", xml}});
}

// Writes project-only fixture metadata covering supported recovery branches.
static void WriteProjectFixtureMetadataArchive(const fs::path &path) {
  static const std::string xml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" provider=\"Test\" providerVersion=\"1\">"
      "<UserData><Data provider=\"Perastage\" ver=\"1.0\">"
      "<ProjectFixtureMetadataMap schemaVersion=\"1.0\">"
      "<ProjectFixtureMetadata uuid=\"20000000-0000-4000-8000-000000000001\" visualColorHex=\"#112233\"/>"
      "<ProjectFixtureMetadata uuid=\"20000000-0000-4000-8000-000000000001\" visualColorHex=\"#445566\"/>"
      "<ProjectFixtureMetadata uuid=\"bad\" visualColorHex=\"#778899\"/>"
      "<ProjectFixtureMetadata uuid=\"20000000-0000-4000-8000-000000000002\" visualColorHex=\"invalid\"/>"
      "<ProjectFixtureMetadata uuid=\"20000000-0000-4000-8000-000000000099\" visualColorHex=\"#AABBCC\"/>"
      "</ProjectFixtureMetadataMap>"
      "<ProjectFixtureMetadataMap schemaVersion=\"2.0\"><ProjectFixtureMetadata uuid=\"20000000-0000-4000-8000-000000000001\" visualColorHex=\"#FFFFFF\"/></ProjectFixtureMetadataMap>"
      "</Data><Data provider=\"Foreign\" ver=\"1.0\"><ProjectFixtureMetadataMap schemaVersion=\"1.0\"><ProjectFixtureMetadata uuid=\"20000000-0000-4000-8000-000000000001\" visualColorHex=\"#000000\"/></ProjectFixtureMetadataMap></Data></UserData>"
      "<Scene><Layers><Layer uuid=\"10000000-0000-4000-8000-000000000001\" name=\"Default\"><ChildList>"
      "<Fixture uuid=\"20000000-0000-4000-8000-000000000001\" name=\"Valid\"><FixtureID>1</FixtureID><FixtureIDNumeric>1</FixtureIDNumeric></Fixture>"
      "<Fixture uuid=\"20000000-0000-4000-8000-000000000002\" name=\"Invalid color\"><FixtureID>2</FixtureID><FixtureIDNumeric>2</FixtureIDNumeric></Fixture>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  WriteArchive(path, {{"GeneralSceneDescription.xml", xml}});
}

// Returns whether the result contains the exact structured diagnostic code.
static bool HasDiagnostic(const MvrImportResult &result,
                          const std::string &code) {
  return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                     [&](const MvrImportDiagnostic &diagnostic) {
                       return diagnostic.code == code;
                     });
}

// Counts exact structured diagnostic codes in deterministic result order.
static size_t CountDiagnostic(const MvrImportResult &result,
                              const std::string &code) {
  return static_cast<size_t>(std::count_if(
      result.diagnostics.begin(), result.diagnostics.end(),
      [&](const MvrImportDiagnostic &diagnostic) {
        return diagnostic.code == code;
      }));
}

// Verifies canonical, legacy, tolerant, and Perastage-extension recovery policy.
static void TestMetadataRecoveryMatrix(const fs::path &tempDir) {
  const fs::path archive = tempDir / "metadata-recovery.mvr";
  WriteMetadataArchive(archive);
  MvrImporter importer;
  MvrImportResult result;
  assert(importer.ImportFromFile(archive.string(), result,
                                 MvrImportMode::ParseOnly, false, false));
  const Support &target =
      result.scene.supports.at("30000000-0000-4000-8000-000000000001");
  assert(target.capacityKg == 100.0f);
  assert(target.weightKg == 0.0f);
  assert(target.motorFixtureUuid ==
         "20000000-0000-4000-8000-000000000001");
  assert(result.scene.supports
             .at("30000000-0000-4000-8000-000000000002")
             .capacityKg == 42.0f);
  const Truss &truss =
      result.scene.trusses.at("40000000-0000-4000-8000-000000000001");
  assert(truss.lengthMm == 0.0f);
  assert(truss.widthMm == 250.0f);
  assert(truss.perastageAuxGdtfArchivePath.empty());
  for (const char *code : {"duplicate_hoist_info", "invalid_hoist_info_uuid",
                           "unknown_hoist_info_uuid", "duplicate_truss_info",
                           "invalid_truss_info_uuid", "unknown_truss_info_uuid",
                           "unsupported_perastage_metadata_version",
                           "legacy_perastage_metadata_missing_version",
                           "invalid_hoist_numeric_field",
                           "invalid_truss_numeric_field",
                           "invalid_use_motor_defaults", "invalid_motor_fixture_uuid",
                           "unknown_motor_fixture_uuid",
                           "unsafe_truss_aux_gdtf_path"}) {
    assert(HasDiagnostic(result, code));
  }
}

// Verifies project-only fixture metadata scope, precedence, and diagnostics.
static void TestProjectFixtureMetadataRecovery(const fs::path &tempDir) {
  const fs::path archive = tempDir / "project-fixture-metadata.mvr";
  WriteProjectFixtureMetadataArchive(archive);
  MvrImporter importer;

  for (const MvrImportSourceKind sourceKind :
       {MvrImportSourceKind::ExternalImport, MvrImportSourceKind::MergeImport}) {
    MvrImportOptions options;
    options.promptConflicts = false;
    options.applyDictionary = false;
    options.sourceKind = sourceKind;
    MvrImportResult ignored;
    assert(importer.ImportFromFile(archive.string(), ignored,
                                   MvrImportMode::ParseOnly, options));
    assert(ignored.scene.fixtures
               .at("20000000-0000-4000-8000-000000000001")
               .visualColorHex.empty());
    assert(!HasDiagnostic(ignored,
                          "duplicate_project_fixture_metadata_uuid"));
  }

  MvrImportOptions projectOptions;
  projectOptions.promptConflicts = false;
  projectOptions.applyDictionary = false;
  projectOptions.sourceKind = MvrImportSourceKind::ProjectRestore;
  MvrImportResult restored;
  assert(importer.ImportFromFile(archive.string(), restored,
                                 MvrImportMode::ParseOnly, projectOptions));
  assert(restored.scene.fixtures
             .at("20000000-0000-4000-8000-000000000001")
             .visualColorHex == "#112233");
  assert(restored.scene.fixtures
             .at("20000000-0000-4000-8000-000000000002")
             .visualColorHex.empty());
  for (const char *code : {
           "duplicate_project_fixture_metadata_uuid",
           "malformed_project_fixture_metadata_uuid",
           "unsupported_project_fixture_metadata_version",
           "unknown_project_fixture_metadata_uuid",
           "invalid_project_fixture_visual_color"}) {
    assert(CountDiagnostic(restored, code) == 1);
  }
}

// Verifies portable, missing, empty, and unsafe AuxGdtf value policy.
static void TestAuxiliaryValuePolicy(const fs::path &tempDir) {
  struct AuxCase {
    std::string value;
    const char *diagnosticCode;
    bool addResource;
    bool retained;
  };
  const std::vector<AuxCase> cases = {
      {"valid.gdtf", "", true, true},
      {"missing.gdtf", "missing_truss_aux_gdtf", false, false},
      {"", "", false, false},
      {"../escape.gdtf", "unsafe_truss_aux_gdtf_path", false, false},
      {"subdir/file.gdtf", "unsafe_truss_aux_gdtf_path", false, false},
      {"/absolute/file.gdtf", "unsafe_truss_aux_gdtf_path", false, false},
      {"C:\\absolute\\file.gdtf", "unsafe_truss_aux_gdtf_path", false,
       false},
      {"\\\\server\\share\\file.gdtf", "unsafe_truss_aux_gdtf_path",
       false, false},
      {"folder\\file.gdtf", "unsafe_truss_aux_gdtf_path", false, false},
      {".", "unsafe_truss_aux_gdtf_path", false, false},
      {"..", "unsafe_truss_aux_gdtf_path", false, false},
      {std::string("control") + static_cast<char>(127) + ".gdtf",
       "unsafe_truss_aux_gdtf_path", false, false}};
  for (size_t index = 0; index < cases.size(); ++index) {
    const AuxCase &testCase = cases[index];
    const fs::path archive =
        tempDir / ("aux-value-" + std::to_string(index) + ".mvr");
    std::vector<std::pair<std::string, std::string>> entries = {
        {"GeneralSceneDescription.xml",
         BuildAuxiliarySceneXml(testCase.value)}};
    if (testCase.addResource)
      entries.emplace_back(testCase.value, "intended-resource");
    WriteArchive(archive, entries);
    MvrImporter importer;
    MvrImportResult result;
    assert(importer.ImportFromFile(archive.string(), result,
                                   MvrImportMode::ParseOnly, false, false));
    const Truss &truss =
        result.scene.trusses.at("40000000-0000-4000-8000-000000000001");
    assert((!truss.perastageAuxGdtfArchivePath.empty()) == testCase.retained);
    assert(truss.manufacturer == "Unrelated");
    if (*testCase.diagnosticCode)
      assert(CountDiagnostic(result, testCase.diagnosticCode) == 1);
  }
}

// Verifies exact and case-only resource collisions never select either payload.
static void TestAmbiguousAuxiliaryResources(const fs::path &tempDir) {
  struct CollisionCase {
    const char *archiveName;
    const char *secondName;
    const char *diagnosticCode;
  };
  for (const CollisionCase &testCase : {
           CollisionCase{"duplicate-resource.mvr", "aux.gdtf",
                         "duplicate_mvr_archive_entry"},
           CollisionCase{"case-resource.mvr", "AUX.GDTF",
                         "case_colliding_mvr_archive_entry"}}) {
    const fs::path archive = tempDir / testCase.archiveName;
    WriteArchive(archive,
                 {{"GeneralSceneDescription.xml",
                   BuildAuxiliarySceneXml("aux.gdtf")},
                  {"aux.gdtf", "first"}, {testCase.secondName, "second"}});
    for (int repetition = 0; repetition < 2; ++repetition) {
      MvrImporter importer;
      MvrImportResult result;
      assert(importer.ImportFromFile(archive.string(), result,
                                     MvrImportMode::ParseOnly, false, false));
      assert(CountDiagnostic(result, testCase.diagnosticCode) == 1);
      const Truss &truss =
          result.scene.trusses.at("40000000-0000-4000-8000-000000000001");
      assert(truss.perastageAuxGdtfArchivePath.empty());
      assert(truss.manufacturer == "Unrelated");
      assert(!fs::exists(fs::path(result.scene.basePath) / "aux.gdtf"));
      assert(!fs::exists(fs::path(result.scene.basePath) / "AUX.GDTF"));
    }
  }
}

// Verifies ambiguous scene descriptions fail while retaining collision diagnostics.
static void TestAmbiguousSceneDescriptions(const fs::path &tempDir) {
  const std::string xml = BuildAuxiliarySceneXml("");
  for (const auto &[archiveName, secondName, code] :
       std::vector<std::tuple<std::string, std::string, std::string>>{
           {"duplicate-scene.mvr", "GeneralSceneDescription.xml",
            "duplicate_mvr_archive_entry"},
           {"case-scene.mvr", "GENERALSCENEDESCRIPTION.XML",
            "case_colliding_mvr_archive_entry"}}) {
    const fs::path archive = tempDir / archiveName;
    WriteArchive(archive, {{"GeneralSceneDescription.xml", xml},
                           {secondName, xml + " "}});
    MvrImporter importer;
    MvrImportResult result;
    assert(!importer.ImportFromFile(archive.string(), result,
                                    MvrImportMode::ParseOnly, false, false));
    assert(CountDiagnostic(result, code) == 1);
    assert(result.scene.trusses.empty());
  }
}

// Runs the GUI-independent metadata recovery test matrix.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  const fs::path tempDir =
      fs::temp_directory_path() / "mvr_perastage_metadata_recovery_test";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  fs::create_directories(tempDir, ec);
  TestMetadataRecoveryMatrix(tempDir);
  TestProjectFixtureMetadataRecovery(tempDir);
  TestAuxiliaryValuePolicy(tempDir);
  TestAmbiguousAuxiliaryResources(tempDir);
  TestAmbiguousSceneDescriptions(tempDir);
  fs::remove_all(tempDir, ec);
  return 0;
}
