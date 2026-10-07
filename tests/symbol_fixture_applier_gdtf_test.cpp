/*
 * This file is part of Perastage.
 */
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
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
#include "../core/fixture_gdtf_derivative_contract.h"
#include "../core/gdtfdictionary.h"
#include "../core/gdtf_mutation_audit.h"
#include "../core/symbols/fixture_symbol_resource_revision.h"
#include "../core/symbols/fixture_symbol_resource_contract.h"
#include "../core/symbols/PerastageSvgSymbol.h"
#include "../core/symbols/Symbol2D.h"
#include "../core/wx_path_utils.h"
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

struct ArchiveSnapshot {
  std::unordered_set<std::string> entries;
  std::unordered_map<std::string, std::string> contents;
  std::string descriptionXml;
};

// Reads archive data while keeping all wxWidgets stream lifetimes inside the helper.
ArchiveSnapshot ReadArchiveSnapshot(const fs::path &archivePath) {
  ArchiveSnapshot snapshot;
  wxFileInputStream input(
      WxPathUtils::WxStringFromFilesystemPath(archivePath));
  assert(input.IsOk());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const auto logicalName = tests::archive::NormalizePresentedArchivePath(
        entry->GetName().ToStdString());
    assert(logicalName.ok);
    snapshot.entries.insert(logicalName.path);
    snapshot.contents[logicalName.path] = ReadCurrentZipEntry(zip);
    if (logicalName.path == "description.xml")
      snapshot.descriptionXml = snapshot.contents.at(logicalName.path);
  }
  return snapshot;
}

// Counts standard revisions created by fixture-symbol application in an archive.
std::size_t CountSymbolMutationRevisions(const fs::path &archivePath) {
  const ArchiveSnapshot snapshot = ReadArchiveSnapshot(archivePath);
  tinyxml2::XMLDocument document;
  assert(document.Parse(snapshot.descriptionXml.c_str(),
                        snapshot.descriptionXml.size()) ==
         tinyxml2::XML_SUCCESS);
  const tinyxml2::XMLElement *fixtureType =
      document.FirstChildElement("GDTF")->FirstChildElement("FixtureType");
  const tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  std::size_t count = 0;
  if (!revisions)
    return count;
  for (const tinyxml2::XMLElement *revision =
           revisions->FirstChildElement("Revision");
       revision; revision = revision->NextSiblingElement("Revision")) {
    const char *text = revision->Attribute("Text");
    if (text && std::string(text) ==
                    "Applied Perastage fixture SVG symbol views (top, side, front, bottom)")
      ++count;
  }
  return count;
}

// Writes a canonical minimal GDTF 1.2 archive for symbol mutation tests.
std::string MakeFixtureGdtf(const fs::path &directory) {
  const fs::path outPath = directory / "SourceFixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(outPath);
  return outPath.filename().string();
}

// Writes a compatibility fixture with caller-provided FixtureType XML.
std::string MakeFixtureGdtfFromFixtureTypeXml(const std::string &fixtureTypeXml,
                                              bool usableFront = true) {
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

  const std::string symbolBody =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
      "<polygon points=\"0,0 10,0 10,10\"/></svg>";
  zipOut.PutNextEntry("models/svg/Body.svg");
  zipOut.Write(symbolBody.data(), symbolBody.size());
  zipOut.PutNextEntry("models/svg/Body_bottom.svg");
  zipOut.Write(symbolBody.data(), symbolBody.size());
  zipOut.PutNextEntry("models/svg_side/Body.svg");
  zipOut.Write(symbolBody.data(), symbolBody.size());
  zipOut.PutNextEntry("models/svg_front/Body.svg");
  const std::string frontBody =
      usableFront ? symbolBody : "<svg viewBox=\"0 0 10 10\"/>";
  zipOut.Write(frontBody.data(), frontBody.size());
  zipOut.Close();

  return outPath;
}

// Writes authored standard views and model offsets without a Bottom extension.
void MakeAuthoredPartialFixture(const fs::path &archivePath, bool usableTop) {
  auto builder = tests::gdtf::BuildMinimalValidFixture();
  builder.WithFixtureIdentity(usableTop ? "AuthoredPartial" : "InvalidAuthored",
                              "Manufacturer",
                              tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
      .WithModelResource("base");
  std::string xml = builder.BuildDescriptionXml();
  const std::string modelStart = "<Model Name=\"Body\"";
  xml.insert(xml.find(modelStart) + modelStart.size(),
             " SVGOffsetX=\"11\" SVGOffsetY=\"12\""
             " SVGSideOffsetX=\"13\" SVGSideOffsetY=\"14\"");
  const std::string svg =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 23 29\">"
      "<polygon points=\"1,2 21,2 21,27\"/></svg>";
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(archivePath));
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  const std::vector<std::pair<std::string, std::string>> entries = {
      {"description.xml", xml},
      {"models/svg/base.svg", usableTop ? svg : "<svg viewBox=\"0 0 23 29\"/>"},
      {"models/svg_side/base.svg", svg}};
  for (const auto &[path, bytes] : entries) {
    zip.PutNextEntry(wxString::FromUTF8(path));
    zip.Write(bytes.data(), bytes.size());
  }
  zip.Close();
}

// Reads archive bytes to prove the external source is unchanged by resolution.
std::string ReadFileBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input.is_open());
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
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

// Restores the active fixture dictionary when the scenario scope exits.
class ScopedFixtureDictionary {
public:
  // Captures the dictionary path that must be restored after the test.
  ScopedFixtureDictionary()
      : previousPath_(GdtfDictionary::GetActiveDictionaryFilePath()) {}

  // Restores the captured dictionary path before temporary files are removed.
  ~ScopedFixtureDictionary() {
    if (!changed_)
      return;
    std::string errorMessage;
    if (!GdtfDictionary::SetActiveDictionaryFilePath(previousPath_, &errorMessage))
      std::cerr << "Could not restore fixture dictionary: " << errorMessage << '\n';
  }

  // Activates a temporary fixture dictionary for the scoped scenarios.
  bool Activate(const fs::path &dictionaryPath, std::string &errorMessage) {
    changed_ = GdtfDictionary::SetActiveDictionaryFilePath(
        dictionaryPath.string(), &errorMessage);
    return changed_;
  }

private:
  std::string previousPath_;
  bool changed_ = false;
};

// Reports a structured application result when an expected condition fails.
void ReportUnexpectedApplyResult(
    const symbol_preview::ApplySymbolsResult &result, bool expected) {
  if (expected)
    return;
  std::cerr << "Apply result: success=" << result.success
            << " sceneUpdated=" << result.sceneUpdated
            << " libraryUpdated=" << result.libraryUpdated
            << " diagnostic='" << result.diagnostic << "'"
            << " scene='" << result.finalScenePath << "'"
            << " library='" << result.finalLibraryPath << "'";
  for (const std::string &warning : result.warnings)
    std::cerr << " warning='" << warning << "'";
  std::cerr << '\n';
}

} // namespace

// Runs the symbol-to-GDTF mutation ownership and compatibility regression test.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();
  ScopedTempProject project;
  ScopedFixtureDictionary dictionaryGuard;
  scene.basePath = project.path.string();

  const std::string gdtfSpec = MakeFixtureGdtf(project.path);
  const std::string gdtfPath = (project.path / gdtfSpec).string();

  Fixture fixture;
  fixture.uuid = "fixture-symbol-test";
  fixture.typeName = "SymbolFixture";
  fixture.gdtfSpec = gdtfSpec;
  scene.fixtures[fixture.uuid] = fixture;
  Fixture sharedFixture = fixture;
  sharedFixture.uuid = "fixture-symbol-shared";
  scene.fixtures[sharedFixture.uuid] = sharedFixture;

  const std::string originalFixtureSpec = fixture.gdtfSpec;
  const fs::path canonicalBeforeFailure =
      project.path / "fixtures" /
      GdtfDictionary::BuildPerastageCanonicalGdtfFileName(gdtfPath);
  fs::create_directories(canonicalBeforeFailure.parent_path());
  const std::string previousPublishedBytes = "previous derivative";
  std::ofstream(canonicalBeforeFailure, std::ios::binary)
      << previousPublishedBytes;
  const auto allSymbols = BuildSymbols();
  const std::vector<symbols::Symbol2D> incompleteSymbols = {allSymbols.front()};
  symbol_preview::ApplySymbolsOptions projectOnlyOptions;
  projectOnlyOptions.updateSceneCopy = true;
  projectOnlyOptions.updateLibraryCopy = false;
  const symbol_preview::ApplySymbolsResult failedPublication =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          incompleteSymbols, fixture.uuid, projectOnlyOptions);
  assert(!failedPublication.success);
  assert(scene.fixtures.at(fixture.uuid).gdtfSpec == originalFixtureSpec);
  assert(scene.fixtures.at(sharedFixture.uuid).gdtfSpec == originalFixtureSpec);
  {
    std::ifstream previousPublishedInput(canonicalBeforeFailure,
                                         std::ios::binary);
    assert(std::string(std::istreambuf_iterator<char>(previousPublishedInput),
                       std::istreambuf_iterator<char>()) ==
           previousPublishedBytes);
  }
  for (const auto &entry : fs::directory_iterator(canonicalBeforeFailure.parent_path()))
    assert(entry.path().filename().string().find(".working.") ==
           std::string::npos);

  symbol_preview::FixtureSymbolInspectionResult before{};
  std::string errorMessage;
  assert(symbol_preview::InspectFixtureSymbolState(fixture, scene, before, errorMessage));
  assert(errorMessage.empty());
  assert(before.hasResolvableGdtf);
  assert(!before.editorIsPerastage);
  assert(before.requiresSymbolGeneration);

  const auto symbols = allSymbols;
  symbol_preview::ApplySymbolsOptions options;
  options.updateSceneCopy = true;
  options.updateLibraryCopy = false;
  const symbol_preview::ApplySymbolsResult sceneResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(symbols, fixture.uuid,
                                                          options);
  ReportUnexpectedApplyResult(
      sceneResult, sceneResult.success && sceneResult.sceneUpdated &&
                       !sceneResult.libraryUpdated &&
                       !sceneResult.finalScenePath.empty() &&
                       !sceneResult.finalSceneFingerprint.empty() &&
                       sceneResult.warnings.empty());
  assert(sceneResult.success);
  assert(sceneResult.sceneUpdated);
  assert(!sceneResult.libraryUpdated);
  assert(!sceneResult.finalScenePath.empty());
  assert(!sceneResult.finalSceneFingerprint.empty());
  assert(sceneResult.warnings.empty());
  assert(scene.fixtures.at(fixture.uuid).gdtfSpec.find("fixtures/") == 0);
  assert(scene.fixtures.at(sharedFixture.uuid).gdtfSpec ==
         scene.fixtures.at(fixture.uuid).gdtfSpec);

  const std::string mutatedPath =
      (project.path / scene.fixtures.at(fixture.uuid).gdtfSpec).string();

  const ArchiveSnapshot mutatedSnapshot =
      ReadArchiveSnapshot(fs::path(mutatedPath));

  std::string derivativeValidationError;
  assert(fixture_gdtf::ValidatePublishedDerivative(
      mutatedPath, derivativeValidationError));
  assert(derivativeValidationError.empty());

  assert(mutatedSnapshot.entries.find("perastage/symbols/Body/top.svg") !=
         mutatedSnapshot.entries.end());
  assert(mutatedSnapshot.entries.find("perastage/symbols/Body/bottom.svg") !=
         mutatedSnapshot.entries.end());
  assert(mutatedSnapshot.entries.find("perastage/symbols/Body/side.svg") !=
         mutatedSnapshot.entries.end());
  assert(mutatedSnapshot.entries.find("perastage/symbols/Body/front.svg") !=
         mutatedSnapshot.entries.end());

  assert(!mutatedSnapshot.entries.contains("models/svg/Body.svg"));
  assert(!mutatedSnapshot.entries.contains("models/svg_side/Body.svg"));
  assert(!mutatedSnapshot.entries.contains("models/svg_front/Body.svg"));
  assert(!mutatedSnapshot.entries.contains("models/svg/Body_bottom.svg"));

  std::string rawNameError;
  const std::vector<std::string> rawNames =
      tests::archive::ReadRawCentralDirectoryEntryNames(mutatedPath, rawNameError);
  assert(rawNameError.empty());
  for (const std::string &expectedName : {"perastage/symbols/Body/top.svg",
                                          "perastage/symbols/Body/bottom.svg",
                                          "perastage/symbols/Body/side.svg",
                                          "perastage/symbols/Body/front.svg"}) {
    assert(std::find(rawNames.begin(), rawNames.end(), expectedName) !=
           rawNames.end());
  }
  for (const std::string &rawName : rawNames) {
    assert(rawName.find('\\') == std::string::npos);
  }

  tinyxml2::XMLDocument doc;
  assert(doc.Parse(mutatedSnapshot.descriptionXml.c_str(),
                   mutatedSnapshot.descriptionXml.size()) ==
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
         "Applied Perastage fixture SVG symbol views (top, side, front, bottom)");
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
  const std::string specBeforeFailedApply =
      scene.fixtures.at(fixture.uuid).gdtfSpec;
  const symbol_preview::ApplySymbolsResult invalidResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          symbols, fixture.uuid, invalidOptions);
  ReportUnexpectedApplyResult(
      invalidResult, !invalidResult.success && !invalidResult.sceneUpdated &&
                         !invalidResult.libraryUpdated &&
                         invalidResult.diagnostic ==
                             "No fixture GDTF persistence target was requested.");
  assert(!invalidResult.success);
  assert(!invalidResult.sceneUpdated);
  assert(!invalidResult.libraryUpdated);
  assert(invalidResult.diagnostic ==
         "No fixture GDTF persistence target was requested.");
  assert(scene.fixtures.at(fixture.uuid).gdtfSpec == specBeforeFailedApply);

  scene.fixtures.at(fixture.uuid).typeName.clear();
  symbol_preview::ApplySymbolsOptions dualOptions;
  dualOptions.updateSceneCopy = true;
  dualOptions.updateLibraryCopy = true;
  const symbol_preview::ApplySymbolsResult libraryFailureResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          symbols, fixture.uuid, dualOptions);
  ReportUnexpectedApplyResult(libraryFailureResult,
                              libraryFailureResult.success &&
                                  libraryFailureResult.sceneUpdated &&
                                  !libraryFailureResult.libraryUpdated &&
                                  !libraryFailureResult.finalSceneFingerprint.empty() &&
                                  libraryFailureResult.warnings.size() == 1);
  assert(libraryFailureResult.success);
  assert(libraryFailureResult.sceneUpdated);
  assert(!libraryFailureResult.libraryUpdated);
  assert(!libraryFailureResult.finalSceneFingerprint.empty());
  assert(libraryFailureResult.warnings.size() == 1);
  assert(scene.fixtures.at(fixture.uuid).gdtfSpec ==
         scene.fixtures.at(sharedFixture.uuid).gdtfSpec);
  assert(fixture_gdtf::ValidatePublishedDerivative(
      libraryFailureResult.finalScenePath, derivativeValidationError));
  scene.fixtures.at(fixture.uuid).typeName = fixture.typeName;

  const fs::path dictionaryPath = project.path / "fixture-symbol-dictionary.json";
  assert(GdtfDictionary::CreateEmptyDictionaryFile(dictionaryPath.string(),
                                                   &errorMessage));
  assert(dictionaryGuard.Activate(dictionaryPath, errorMessage));

  const symbol_preview::ApplySymbolsResult dualResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          symbols, fixture.uuid, dualOptions);
  ReportUnexpectedApplyResult(
      dualResult, dualResult.success && dualResult.sceneUpdated &&
                      dualResult.libraryUpdated &&
                      !dualResult.finalScenePath.empty() &&
                      !dualResult.finalLibraryPath.empty());
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
  ReportUnexpectedApplyResult(
      libraryOnlyResult,
      libraryOnlyResult.success && !libraryOnlyResult.sceneUpdated &&
          libraryOnlyResult.libraryUpdated &&
          libraryOnlyResult.finalScenePath.empty());
  assert(libraryOnlyResult.success);
  assert(!libraryOnlyResult.sceneUpdated);
  assert(libraryOnlyResult.libraryUpdated);
  assert(libraryOnlyResult.finalScenePath.empty());

  const fs::path dictionaryAssets =
      project.path / "fixture-symbol-dictionary_assets";
  fs::create_directories(dictionaryAssets);
  const fs::path sameFileArchive =
      dictionaryAssets /
      GdtfDictionary::BuildPerastageCanonicalGdtfFileName(gdtfPath);
  fs::copy_file(gdtfPath, sameFileArchive,
                fs::copy_options::overwrite_existing);
  Fixture sameFileFixture;
  sameFileFixture.uuid = "fixture-symbol-same-file";
  sameFileFixture.typeName = "SameFileSymbolFixture";
  sameFileFixture.gdtfSpec =
      fs::relative(sameFileArchive, project.path).generic_string();
  scene.fixtures[sameFileFixture.uuid] = sameFileFixture;
  const std::size_t revisionsBefore =
      CountSymbolMutationRevisions(sameFileArchive);
  symbol_cache::ClearGdtfSemanticFingerprintCache();
  const symbol_preview::ApplySymbolsResult sameFileResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          symbols, sameFileFixture.uuid, dualOptions);
  ReportUnexpectedApplyResult(
      sameFileResult, sameFileResult.success && sameFileResult.sceneUpdated &&
                          sameFileResult.libraryUpdated);
  assert(sameFileResult.success);
  assert(sameFileResult.sceneUpdated);
  assert(sameFileResult.libraryUpdated);
  std::error_code equivalenceError;
  assert(!fs::equivalent(sameFileResult.finalScenePath,
                         sameFileResult.finalLibraryPath, equivalenceError));
  assert(!equivalenceError);
  assert(fixture_gdtf::ValidatePublishedDerivative(
      sameFileResult.finalScenePath, derivativeValidationError));
  assert(fixture_gdtf::ValidatePublishedDerivative(
      sameFileResult.finalLibraryPath, derivativeValidationError));
  assert(CountSymbolMutationRevisions(sameFileArchive) == revisionsBefore + 1);
  assert(!InspectFixturePath("fixture-same-file-inspection",
                             sameFileArchive.string())
              .requiresSymbolGeneration);
  std::string fingerprintError;
  assert(symbol_cache::ComputeGdtfSemanticFingerprint(
             sameFileArchive.string(), fingerprintError) ==
         sameFileResult.finalSceneFingerprint);
  assert(fingerprintError.empty());

  const fs::path authoredSource = project.path / "AuthoredPartial.gdtf";
  MakeAuthoredPartialFixture(authoredSource, true);
  const std::string authoredSourceBytes = ReadFileBytes(authoredSource);
  const ArchiveSnapshot authoredBefore = ReadArchiveSnapshot(authoredSource);
  Fixture authoredFixture;
  authoredFixture.uuid = "fixture-authored-partial";
  authoredFixture.typeName = "AuthoredPartial";
  authoredFixture.gdtfSpec = authoredSource.filename().string();
  scene.fixtures[authoredFixture.uuid] = authoredFixture;
  const auto authoredBeforeInspection =
      InspectFixturePath(authoredFixture.uuid, authoredSource.string());
  assert(!authoredBeforeInspection.hasValidSvgSymbolSet);
  assert(authoredBeforeInspection.requiresSymbolGeneration);
  auto authoredSymbols = symbols;
  authoredSymbols.front().bounds.min = {3.0f, 4.0f};
  authoredSymbols.front().bounds.max = {103.0f, 54.0f};
  authoredSymbols.front().strokes = {{{3.0f, 4.0f}, {103.0f, 54.0f}}};
  const auto authoredResult = symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
      authoredSymbols, authoredFixture.uuid, projectOnlyOptions);
  ReportUnexpectedApplyResult(authoredResult, authoredResult.success);
  assert(authoredResult.success);
  assert(ReadFileBytes(authoredSource) == authoredSourceBytes);
  const ArchiveSnapshot authoredAfter = ReadArchiveSnapshot(authoredResult.finalScenePath);
  for (const std::string &path : {"models/svg/base.svg", "models/svg_side/base.svg"})
    assert(authoredAfter.contents.at(path) == authoredBefore.contents.at(path));

  tinyxml2::XMLDocument authoredDocument;
  assert(authoredDocument.Parse(authoredAfter.descriptionXml.c_str()) ==
         tinyxml2::XML_SUCCESS);
  const auto *authoredModel = authoredDocument.FirstChildElement("GDTF")
                                  ->FirstChildElement("FixtureType")
                                  ->FirstChildElement("Models")
                                  ->FirstChildElement("Model");
  assert(authoredModel->FloatAttribute("SVGOffsetX") == 11.0f);
  assert(authoredModel->FloatAttribute("SVGOffsetY") == 12.0f);
  assert(authoredModel->FloatAttribute("SVGSideOffsetX") == 13.0f);
  assert(authoredModel->FloatAttribute("SVGSideOffsetY") == 14.0f);
  FixtureSymbolResourceInspection authoredResources;
  assert(InspectFixtureSymbolResources(authoredResult.finalScenePath, authoredResources));
  assert(!authoredResources.standardViewsUsable);
  assert(authoredResources.perastageViewsUsable);
  assert(authoredResources.FindStandardView(SymbolViewKind::Top)->provenance ==
         FixtureSymbolProvenance::AuthoredGdtf);
  assert(authoredResources.FindStandardView(SymbolViewKind::Left)->provenance ==
         FixtureSymbolProvenance::AuthoredGdtf);
  assert(!authoredResources.FindStandardView(SymbolViewKind::Front)->exists);
  assert(!authoredAfter.entries.contains("models/svg_front/base.svg"));
  assert(!authoredAfter.entries.contains("models/svg/base_bottom.svg"));
  for (const SymbolViewKind view : {SymbolViewKind::Top, SymbolViewKind::Left,
                                   SymbolViewKind::Front, SymbolViewKind::Bottom}) {
    const auto *resource = authoredResources.FindPerastageView(view);
    assert(resource->provenance == FixtureSymbolProvenance::GeneratedPerastage);
    assert(!resource->standardGdtf);
    const std::string &path = resource->archivePath;
    tinyxml2::XMLDocument generatedDocument;
    assert(generatedDocument.Parse(authoredAfter.contents.at(path).c_str()) ==
           tinyxml2::XML_SUCCESS);
    assert(generatedDocument.FirstChildElement("svg")->IntAttribute(
               kPerastageSymbolVersionAttribute) ==
           kCurrentPerastageSymbolResourceVersion);
  }

  PerastageSvgSymbolData internalTop;
  std::string internalTopError;
  assert(LoadPerastageSvgSymbolFromGdtf(authoredResult.finalScenePath,
                                      SymbolViewKind::Top, internalTop,
                                      &internalTopError));
  assert(internalTopError.empty());
  assert(internalTop.sourcePath == "perastage/symbols/base/top.svg");
  assert(internalTop.offsetXmm == -3.0);
  assert(internalTop.offsetYmm == -4.0);

  const fs::path invalidAuthoredSource = project.path / "InvalidAuthored.gdtf";
  MakeAuthoredPartialFixture(invalidAuthoredSource, false);
  const std::string invalidAuthoredSourceBytes = ReadFileBytes(invalidAuthoredSource);
  Fixture invalidAuthoredFixture = authoredFixture;
  invalidAuthoredFixture.uuid = "fixture-invalid-authored-partial";
  invalidAuthoredFixture.typeName = "InvalidAuthored";
  invalidAuthoredFixture.gdtfSpec = invalidAuthoredSource.filename().string();
  scene.fixtures[invalidAuthoredFixture.uuid] = invalidAuthoredFixture;
  const auto invalidAuthoredResult = symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
      symbols, invalidAuthoredFixture.uuid, projectOnlyOptions);
  ReportUnexpectedApplyResult(invalidAuthoredResult, invalidAuthoredResult.success);
  assert(invalidAuthoredResult.success);
  assert(ReadFileBytes(invalidAuthoredSource) == invalidAuthoredSourceBytes);
  const auto invalidAuthoredBefore = ReadArchiveSnapshot(invalidAuthoredSource);
  const auto invalidAuthoredAfter = ReadArchiveSnapshot(invalidAuthoredResult.finalScenePath);
  assert(invalidAuthoredAfter.contents.at("models/svg/base.svg") ==
         invalidAuthoredBefore.contents.at("models/svg/base.svg"));
  FixtureSymbolResourceInspection invalidAuthoredResources;
  assert(InspectFixtureSymbolResources(invalidAuthoredResult.finalScenePath,
                                      invalidAuthoredResources));
  assert(!invalidAuthoredResources.FindStandardView(SymbolViewKind::Top)->usable);
  assert(invalidAuthoredResources.FindStandardView(SymbolViewKind::Top)->provenance ==
         FixtureSymbolProvenance::AuthoredGdtf);
  assert(invalidAuthoredResources.perastageViewsUsable);

  const fs::path standardOnlySource = project.path / "StandardViewsOnly.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("StandardViewsOnly", "Manufacturer",
                           tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
      .WriteArchive(standardOnlySource);
  Fixture standardOnlyFixture;
  standardOnlyFixture.uuid = "fixture-standard-only";
  standardOnlyFixture.typeName = "StandardViewsOnly";
  standardOnlyFixture.gdtfSpec = standardOnlySource.filename().string();
  scene.fixtures[standardOnlyFixture.uuid] = standardOnlyFixture;
  std::vector<symbols::Symbol2D> standardSymbols;
  for (const auto &symbol : symbols) {
    if (symbol.view != symbols::SymbolView::Bottom)
      standardSymbols.push_back(symbol);
  }
  const auto standardOnlyResult = symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
      standardSymbols, standardOnlyFixture.uuid, projectOnlyOptions);
  assert(!standardOnlyResult.success);
  assert(!standardOnlyResult.diagnostic.empty());
  assert(scene.fixtures.at(standardOnlyFixture.uuid).gdtfSpec ==
         standardOnlyFixture.gdtfSpec);

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

  const std::string externalVersionPath = MakeFixtureGdtfFromFixtureTypeXml(
      "<FixtureType Name=\"External\" Manufacturer=\"Acme\" Editor=\"Vendor\">"
      "<Models><Model Name=\"Body\" File=\"\" PrimitiveType=\"Cube\"/></Models>"
      "</FixtureType>");
  const auto externalVersion =
      InspectFixturePath("fixture-external-version", externalVersionPath);
  assert(!externalVersion.editorIsPerastage);
  assert(externalVersion.hasValidSvgSymbolSet);
  assert(!externalVersion.requiresSymbolGeneration);

  const std::string invalidExternalPath = MakeFixtureGdtfFromFixtureTypeXml(
      "<FixtureType Name=\"InvalidExternal\" Manufacturer=\"Acme\" Editor=\"Vendor\">"
      "<Models><Model Name=\"Body\" File=\"\" PrimitiveType=\"Cube\"/></Models>"
      "</FixtureType>",
      false);
  const auto invalidExternal =
      InspectFixturePath("fixture-invalid-external", invalidExternalPath);
  assert(!invalidExternal.editorIsPerastage);
  assert(!invalidExternal.hasValidSvgSymbolSet);
  assert(invalidExternal.requiresSymbolGeneration);

  const std::string unknownVersionPath = MakeFixtureGdtfFromFixtureTypeXml(
      "<FixtureType Name=\"Future\" Manufacturer=\"Acme\" Editor=\"Perastage\">"
      "<Models><Model Name=\"Body\" File=\"\" PrimitiveType=\"Cube\"/></Models>"
      "<PerastageMutationAudit SchemaVersion=\"999\"/>"
      "</FixtureType>");
  const auto unknownVersion =
      InspectFixturePath("fixture-unknown-version", unknownVersionPath);
  assert(!unknownVersion.editorIsPerastage);
  assert(unknownVersion.hasValidSvgSymbolSet);
  assert(!unknownVersion.requiresSymbolGeneration);
  assert(!unknownVersion.warningMessage.empty());

  std::vector<GdtfObject> objects;
  std::string loadError;
  assert(LoadGdtf(mutatedPath, objects, &loadError));
  assert(loadError.empty());

  std::error_code ec;
  fs::remove(currentVersionPath, ec);
  fs::remove(externalVersionPath, ec);
  fs::remove(invalidExternalPath, ec);
  fs::remove(unknownVersionPath, ec);
  cfg.Reset();
  return 0;
}
