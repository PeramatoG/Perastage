/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <tinyxml2.h>
#include <wx/filename.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "../core/configmanager.h"
#include "../core/gdtf_mutation_audit.h"
#include "../core/symbols/Symbol2D.h"
#include "../gui/windows/symbol_fixture_applier.h"
#include "../models/fixture.h"
#include "../viewer3d/gdtfloader.h"

namespace fs = std::filesystem;

namespace {

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

std::string MakeFixtureGdtf() {
  wxFileName tempName(wxFileName::CreateTempFileName("gdtf_symbol_apply_"));
  const std::string outPath = tempName.GetFullPath().ToStdString() + ".gdtf";
  wxRemoveFile(tempName.GetFullPath());

  wxFFileOutputStream fileOut(outPath);
  assert(fileOut.IsOk());
  wxZipOutputStream zipOut(fileOut);

  zipOut.PutNextEntry("description.xml");
  const std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GDTF DataVersion=\"1.2\">"
      "<FixtureType Name=\"SymbolFixture\" Manufacturer=\"Acme\" Editor=\"Vendor\">"
      "<Models>"
      "<Model Name=\"Body\" File=\"\" PrimitiveType=\"Cube\" Length=\"1\" Width=\"1\" Height=\"1\"/>"
      "</Models>"
      "<Geometries><Geometry Name=\"Root\" Model=\"Body\"/></Geometries>"
      "</FixtureType>"
      "</GDTF>";
  zipOut.Write(xml.data(), xml.size());
  zipOut.Close();

  return outPath;
}

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
      makeView(symbols::SymbolView::Left),
      makeView(symbols::SymbolView::Front),
  };
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();

  const std::string gdtfPath = MakeFixtureGdtf();

  Fixture fixture;
  fixture.uuid = "fixture-symbol-test";
  fixture.typeName = "SymbolFixture";
  fixture.gdtfSpec = gdtfPath;
  scene.fixtures[fixture.uuid] = fixture;

  symbol_preview::FixtureSymbolInspectionResult before{};
  std::string errorMessage;
  assert(symbol_preview::InspectFixtureSymbolState(fixture, scene, before, errorMessage));
  assert(errorMessage.empty());
  assert(before.hasResolvableGdtf);
  assert(!before.editorIsPerastage);

  const auto symbols = BuildSymbols();
  symbol_preview::ApplySymbolsOptions options;
  options.updateSceneCopy = true;
  options.updateLibraryCopy = false;
  assert(symbol_preview::ApplySymbolsToFixtureGdtf(symbols, fixture.uuid, errorMessage,
                                                   options));
  assert(errorMessage.empty());

  wxFileInputStream input(gdtfPath);
  assert(input.IsOk());
  wxZipInputStream zipInput(input);

  std::unordered_set<std::string> entries;
  std::string descriptionXml;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const std::string entryName = entry->GetName().ToStdString();
    entries.insert(entryName);
    if (entryName == "description.xml")
      descriptionXml = ReadCurrentZipEntry(zipInput);
  }

  assert(entries.find("models/svg/Body.svg") != entries.end());
  assert(entries.find("models/svg_side/Body.svg") != entries.end());
  assert(entries.find("models/svg_front/Body.svg") != entries.end());

  tinyxml2::XMLDocument doc;
  assert(doc.Parse(descriptionXml.c_str(), descriptionXml.size()) ==
         tinyxml2::XML_SUCCESS);

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  assert(fixtureType != nullptr);
  fixtureType = fixtureType->FirstChildElement("FixtureType");
  assert(fixtureType != nullptr);

  const char *editor = fixtureType->Attribute("Editor");
  assert(editor != nullptr);
  assert(std::string(editor) == "Perastage");

  const bool hasRevision =
      fixtureType->FirstChildElement("Revisions") != nullptr &&
      fixtureType->FirstChildElement("Revisions")->FirstChildElement("Revision") != nullptr;
  assert(hasRevision);

  tinyxml2::XMLElement *audit = fixtureType->FirstChildElement("PerastageMutationAudit");
  assert(audit != nullptr);
  assert(audit->IntAttribute("SchemaVersion") ==
         GdtfMutationAudit::kPerastageGdtfMutationSchemaVersion);

  tinyxml2::XMLElement *revision =
      fixtureType->FirstChildElement("Revisions")->FirstChildElement("Revision");
  assert(revision != nullptr);
  const char *text = revision->Attribute("Text");
  const char *modifiedBy = revision->Attribute("ModifiedBy");
  assert(text != nullptr);
  assert(modifiedBy != nullptr);
  assert(std::string(text) ==
         "Applied fixture SVG symbol views (top, side, front)");
  assert(std::string(modifiedBy).rfind("Perastage ", 0) == 0);

  symbol_preview::FixtureSymbolInspectionResult after{};
  assert(symbol_preview::InspectFixtureSymbolState(scene.fixtures.at(fixture.uuid), scene,
                                                   after, errorMessage));
  assert(errorMessage.empty());
  assert(after.editorIsPerastage);
  assert(after.hasValidSvgSymbolSet);
  assert(!after.requiresSymbolGeneration);

  std::vector<GdtfObject> objects;
  std::string loadError;
  assert(LoadGdtf(gdtfPath, objects, &loadError));
  assert(loadError.empty());

  std::error_code ec;
  fs::remove(gdtfPath, ec);
  cfg.Reset();
  return 0;
}
