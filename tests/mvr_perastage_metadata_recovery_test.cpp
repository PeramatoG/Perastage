/*
 * This file is part of Perastage.
 */
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <memory>
#include <string>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "mvrimporter.h"

namespace fs = std::filesystem;

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
  wxFileOutputStream output(path.generic_string());
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  assert(zip.PutNextEntry("GeneralSceneDescription.xml"));
  zip.Write(xml.data(), xml.size());
  zip.Close();
}

// Returns whether the result contains the exact structured diagnostic code.
static bool HasDiagnostic(const MvrImportResult &result,
                          const std::string &code) {
  return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                     [&](const MvrImportDiagnostic &diagnostic) {
                       return diagnostic.code == code;
                     });
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
  fs::remove_all(tempDir, ec);
  return 0;
}
