/*
 * This file is part of Perastage.
 */
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <tinyxml2.h>
#include <wx/filename.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"

#include "../core/configmanager.h"
#include "../core/gdtfdictionary.h"
#include "../core/gdtf_mutation_audit.h"
#include "../core/symbols/Symbol2D.h"
#include "../gui/windows/symbol_fixture_applier.h"
#include "../models/fixture.h"
#include "../viewer3d/gdtfloader.h"

namespace fs = std::filesystem;

namespace {

// Reads the current ZIP entry contents as bytes.
std::string ReadCurrentZipEntry(wxZipInputStream &zip) {
  std::string content;
  char buffer[4096];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t bytes = zip.LastRead();
    if (bytes == 0)
      break;
    content.append(buffer, bytes);
  }
  return content;
}

// Writes a canonical minimal GDTF 1.2 archive for symbol mutation tests.
std::string MakeFixtureGdtf(const fs::path &directory) {
  const fs::path outPath = directory / "SourceFixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(outPath);
  return outPath.filename().string();
}

// Writes a compatibility fixture with caller-provided FixtureType XML.
std::string MakeFixtureGdtfFromFixtureTypeXml(const std::string &fixtureTypeXml) {
  wxFileName tempName(wxFileName::CreateTempFileName("gdtf_symbol_compat_"));
  const std::string outPath = tempName.GetFullPath().ToStdString() + ".gdtf";
  wxRemoveFile(tempName.GetFullPath());

  wxFFileOutputStream fileOut(outPath);
  assert(fileOut.IsOk());
  wxZipOutputStream zipOut(fileOut);

  zipOut.PutNextEntry("description.xml");
  const std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                          "<GDTF DataVersion=\"1.2\">" +
                          fixtureTypeXml + "</GDTF>";
  zipOut.Write(xml.data(), xml.size());

  const std::string symbolBody = "<svg xmlns=\"http://www.w3.org/2000/svg\"></svg>";
  zipOut.PutNextEntry("models/svg/Body.svg");
  zipOut.Write(symbolBody.data(), symbolBody.size());
  zipOut.PutNextEntry("models/svg/Body_bottom.svg");
  zipOut.Write(symbolBody.data(), symbolBody.size());
  zipOut.PutNextEntry("models/svg_side/Body.svg");
  zipOut.Write(symbolBody.data(), symbolBody.size());
  zipOut.PutNextEntry("models/svg_front/Body.svg");
  zipOut.Write(symbolBody.data(), symbolBody.size());
  zipOut.Close();

  return outPath;
}

// Inspects symbol compatibility for a fixture path in the current scene.
symbol_preview::FixtureSymbolInspectionResult InspectFixturePath(const std::string &fixtureUuid,
                                                                 const std::string &gdtfPath) {
  auto &cfg = ConfigManager::Get();
  Fixture fixture;
  fixture.uuid = fixtureUuid;
  fixture.typeName = "SymbolFixture";
  fixture.gdtfSpec = gdtfPath;
  cfg.GetScene().fixtures[fixture.uuid] = fixture;

  symbol_preview::FixtureSymbolInspectionResult inspection{};
  std::string errorMessage;
  assert(symbol_preview::InspectFixtureSymbolState(fixture, cfg.GetScene(), inspection,
                                                   errorMessage));
  assert(errorMessage.empty());
  return inspection;
}

// Builds one simple symbol payload for each supported fixture view.
std::vector<symbols::Symbol2D> BuildSymbols() {
  auto makeView = [](symbols::SymbolView view) {
    symbols::Symbol2D symbol;
    symbol.view = view;
    symbol.bounds.min = {0.0f, 0.0f};
    symbol.bounds.max = {100.0f, 50.0f};
    symbol.bounds.valid = true;
    symbol.strokes.push_back({{0.0f, 0.0f}, {100.0f, 50.0f}});
    return symbol;
  };

  return {
      makeView(symbols::SymbolView::Top),
      makeView(symbols::SymbolView::Bottom),
      makeView(symbols::SymbolView::Left),
      makeView(symbols::SymbolView::Front),
  };
}

// Removes a temporary project directory when the test scope exits.
class ScopedTempProject {
public:
  // Creates a unique temporary project directory.
  ScopedTempProject() {
    path = fs::temp_directory_path() /
           (std::string("symbol_fixture_project_") +
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    fs::create_directories(path);
  }
  // Removes the temporary project directory.
  ~ScopedTempProject() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  fs::path path;
};

} // namespace

// Runs the symbol-to-GDTF mutation ownership and compatibility regression test.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();
  ScopedTempProject project;
  scene.basePath = project.path.string();

  const std::string gdtfSpec = MakeFixtureGdtf(project.path);
  const std::string gdtfPath = (project.path / gdtfSpec).string();

  Fixture fixture;
  fixture.uuid = "fixture-symbol-test";
  fixture.typeName = "SymbolFixture";
  fixture.gdtfSpec = gdtfSpec;
  scene.fixtures[fixture.uuid] = fixture;

  symbol_preview::FixtureSymbolInspectionResult before{};
  std::string errorMessage;
  assert(symbol_preview::InspectFixtureSymbolState(fixture, scene, before, errorMessage));
  assert(errorMessage.empty());
  assert(before.hasResolvableGdtf);
  assert(!before.editorIsPerastage);
  assert(before.requiresSymbolGeneration);

  const auto symbols = BuildSymbols();
  symbol_preview::ApplySymbolsOptions options;
  options.updateSceneCopy = true;
  options.updateLibraryCopy = false;
  const symbol_preview::ApplySymbolsResult sceneResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(symbols, fixture.uuid,
                                                          options);
  if (!sceneResult.success) {
    std::cerr << "ApplySymbolsToFixtureGdtf failed: " << sceneResult.diagnostic
              << std::endl;
    assert(false);
  }
  assert(sceneResult.sceneUpdated);
  assert(!sceneResult.libraryUpdated);
  assert(!sceneResult.finalScenePath.empty());
  assert(!sceneResult.finalSceneFingerprint.empty());
  assert(sceneResult.warnings.empty());
  assert(scene.fixtures.at(fixture.uuid).gdtfSpec.find("fixtures/") == 0);

  const std::string mutatedPath =
      (project.path / scene.fixtures.at(fixture.uuid).gdtfSpec).string();

  wxFileInputStream input(mutatedPath);
  assert(input.IsOk());
  wxZipInputStream zipInput(input);

  std::unordered_set<std::string> entries;
  std::string descriptionXml;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const auto logicalName =
        tests::archive::NormalizePresentedArchivePath(entry->GetName().ToStdString());
    assert(logicalName.ok);
    entries.insert(logicalName.path);
    if (logicalName.path == "description.xml")
      descriptionXml = ReadCurrentZipEntry(zipInput);
  }

  assert(entries.find("models/svg/Body.svg") != entries.end());
  assert(entries.find("models/svg/Body_bottom.svg") != entries.end());
  assert(entries.find("models/svg_side/Body.svg") != entries.end());
  assert(entries.find("models/svg_front/Body.svg") != entries.end());

  std::string rawNameError;
  const std::vector<std::string> rawNames =
      tests::archive::ReadRawCentralDirectoryEntryNames(mutatedPath, rawNameError);
  assert(rawNameError.empty());
  for (const std::string &expectedName : {"models/svg/Body.svg",
                                          "models/svg/Body_bottom.svg",
                                          "models/svg_side/Body.svg",
                                          "models/svg_front/Body.svg"}) {
    assert(std::find(rawNames.begin(), rawNames.end(), expectedName) !=
           rawNames.end());
  }
  for (const std::string &rawName : rawNames) {
    assert(rawName.find('\\') == std::string::npos);
  }

  tinyxml2::XMLDocument doc;
  assert(doc.Parse(descriptionXml.c_str(), descriptionXml.size()) ==
         tinyxml2::XML_SUCCESS);

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  assert(fixtureType != nullptr);
  fixtureType = fixtureType->FirstChildElement("FixtureType");
  assert(fixtureType != nullptr);

  const char *editor = fixtureType->Attribute("Editor");
  assert(editor == nullptr);

  const bool hasRevision =
      fixtureType->FirstChildElement("Revisions") != nullptr &&
      fixtureType->FirstChildElement("Revisions")->FirstChildElement("Revision") != nullptr;
  assert(hasRevision);

  tinyxml2::XMLElement *audit = fixtureType->FirstChildElement("PerastageMutationAudit");
  assert(audit == nullptr);

  tinyxml2::XMLElement *revision =
      fixtureType->FirstChildElement("Revisions")->FirstChildElement("Revision");
  assert(revision != nullptr);
  const char *date = revision->Attribute("Date");
  const char *text = revision->Attribute("Text");
  const char *modifiedBy = revision->Attribute("ModifiedBy");
  assert(date != nullptr && std::string(date).size() > 0);
  assert(text != nullptr);
  assert(modifiedBy != nullptr);
  assert(std::string(text) ==
         "Applied fixture SVG symbol views (top, side, front, bottom)");
  assert(std::string(modifiedBy).rfind("Perastage ", 0) == 0);

  symbol_preview::FixtureSymbolInspectionResult after{};
  assert(symbol_preview::InspectFixtureSymbolState(scene.fixtures.at(fixture.uuid), scene,
                                                   after, errorMessage));
  assert(errorMessage.empty());
  assert(after.editorIsPerastage);
  assert(after.hasValidSvgSymbolSet);
  assert(!after.requiresSymbolGeneration);

  symbol_preview::ApplySymbolsOptions invalidOptions;
  invalidOptions.updateSceneCopy = false;
  invalidOptions.updateLibraryCopy = false;
  const symbol_preview::ApplySymbolsResult invalidResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          symbols, fixture.uuid, invalidOptions);
  assert(!invalidResult.success);
  assert(!invalidResult.sceneUpdated);
  assert(!invalidResult.libraryUpdated);
  assert(invalidResult.diagnostic ==
         "No fixture GDTF persistence target was requested.");

  scene.fixtures.at(fixture.uuid).typeName.clear();
  symbol_preview::ApplySymbolsOptions dualOptions;
  dualOptions.updateSceneCopy = true;
  dualOptions.updateLibraryCopy = true;
  const symbol_preview::ApplySymbolsResult libraryFailureResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          symbols, fixture.uuid, dualOptions);
  assert(libraryFailureResult.success);
  assert(libraryFailureResult.sceneUpdated);
  assert(!libraryFailureResult.libraryUpdated);
  assert(!libraryFailureResult.finalSceneFingerprint.empty());
  assert(libraryFailureResult.warnings.size() == 1);
  scene.fixtures.at(fixture.uuid).typeName = fixture.typeName;

  const std::string previousDictionaryPath =
      GdtfDictionary::GetActiveDictionaryFilePath();
  const fs::path dictionaryPath = project.path / "fixture-symbol-dictionary.json";
  assert(GdtfDictionary::CreateEmptyDictionaryFile(dictionaryPath.string(),
                                                   &errorMessage));
  assert(GdtfDictionary::SetActiveDictionaryFilePath(dictionaryPath.string(),
                                                     &errorMessage));

  const symbol_preview::ApplySymbolsResult dualResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          symbols, fixture.uuid, dualOptions);
  assert(dualResult.success);
  assert(dualResult.sceneUpdated);
  assert(dualResult.libraryUpdated);
  assert(!dualResult.finalScenePath.empty());
  assert(!dualResult.finalLibraryPath.empty());
  assert(scene.fixtures.at(fixture.uuid).gdtfSpec.find("fixtures/") == 0);
  assert(!InspectFixturePath("fixture-library-inspection",
                             dualResult.finalLibraryPath)
              .requiresSymbolGeneration);

  symbol_preview::ApplySymbolsOptions libraryOnlyOptions;
  libraryOnlyOptions.updateSceneCopy = false;
  libraryOnlyOptions.updateLibraryCopy = true;
  const symbol_preview::ApplySymbolsResult libraryOnlyResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          symbols, fixture.uuid, libraryOnlyOptions);
  assert(libraryOnlyResult.success);
  assert(!libraryOnlyResult.sceneUpdated);
  assert(libraryOnlyResult.libraryUpdated);
  assert(libraryOnlyResult.finalScenePath.empty());

  assert(GdtfDictionary::SetActiveDictionaryFilePath(previousDictionaryPath,
                                                     &errorMessage));

  const std::string currentVersionPath = MakeFixtureGdtfFromFixtureTypeXml(
      "<FixtureType Name=\"Current\" Manufacturer=\"Acme\" Editor=\"Vendor\">"
      "<Models><Model Name=\"Body\" File=\"\" PrimitiveType=\"Cube\"/></Models>"
      "<PerastageMutationAudit SchemaVersion=\"1\"/>"
      "</FixtureType>");
  const auto currentVersion =
      InspectFixturePath("fixture-current-version", currentVersionPath);
  assert(currentVersion.editorIsPerastage);
  assert(currentVersion.hasValidSvgSymbolSet);
  assert(!currentVersion.requiresSymbolGeneration);
  assert(currentVersion.warningMessage.empty());

  const std::string unknownVersionPath = MakeFixtureGdtfFromFixtureTypeXml(
      "<FixtureType Name=\"Future\" Manufacturer=\"Acme\" Editor=\"Perastage\">"
      "<Models><Model Name=\"Body\" File=\"\" PrimitiveType=\"Cube\"/></Models>"
      "<PerastageMutationAudit SchemaVersion=\"999\"/>"
      "</FixtureType>");
  const auto unknownVersion =
      InspectFixturePath("fixture-unknown-version", unknownVersionPath);
  assert(!unknownVersion.editorIsPerastage);
  assert(!unknownVersion.hasValidSvgSymbolSet);
  assert(unknownVersion.requiresSymbolGeneration);
  assert(!unknownVersion.warningMessage.empty());

  std::vector<GdtfObject> objects;
  std::string loadError;
  assert(LoadGdtf(mutatedPath, objects, &loadError));
  assert(loadError.empty());

  std::error_code ec;
  fs::remove(currentVersionPath, ec);
  fs::remove(unknownVersionPath, ec);
  cfg.Reset();
  return 0;
}
