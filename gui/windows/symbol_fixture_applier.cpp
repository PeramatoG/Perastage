#include "windows/symbol_fixture_applier.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "guiconfigservices.h"
#include "projectutils.h"
#include "windows/symbol_preview_exporter.h"

namespace fs = std::filesystem;

namespace symbol_preview {
namespace {

struct SymbolPayload {
  std::string archivePath;
  std::string svg;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
};

bool ReadAllBytes(wxZipInputStream &zip, std::string &out) {
  out.clear();
  char buffer[4096];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t count = zip.LastRead();
    if (count == 0)
      break;
    out.append(buffer, count);
  }
  return true;
}

std::string ResolveGdtfPath(const Fixture &fixture, const MvrScene &scene) {
  if (fixture.gdtfSpec.empty())
    return {};

  const fs::path specPath = fs::path(fixture.gdtfSpec);
  std::error_code ec;
  if (specPath.is_absolute() && fs::exists(specPath, ec) && !ec)
    return specPath.string();

  if (!scene.basePath.empty()) {
    fs::path localPath = fs::path(scene.basePath) / specPath;
    ec.clear();
    if (fs::exists(localPath, ec) && !ec)
      return localPath.string();
  }

  fs::path libraryPath =
      fs::path(ProjectUtils::GetDefaultLibraryPath("fixtures")) / specPath.filename();
  ec.clear();
  if (fs::exists(libraryPath, ec) && !ec)
    return libraryPath.string();

  return {};
}

const symbols::Symbol2D *FindSymbol(const std::vector<symbols::Symbol2D> &symbols,
                                    symbols::SymbolView view) {
  for (const auto &symbol : symbols) {
    if (symbol.view == view)
      return &symbol;
  }
  return nullptr;
}

bool BuildSymbolPayload(const std::vector<symbols::Symbol2D> &symbols,
                        symbols::SymbolView view,
                        const std::string &archivePath,
                        SymbolPayload &out,
                        std::string &errorMessage) {
  const symbols::Symbol2D *symbol = FindSymbol(symbols, view);
  if (!symbol || !symbol->bounds.valid)
    return false;

  out.archivePath = archivePath;
  out.offsetX = -symbol->bounds.min.x;
  out.offsetY = -symbol->bounds.min.y;
  if (!symbol_preview::ExportSymbolToSvgString(*symbol, out.svg, errorMessage))
    return false;

  return true;
}

bool PatchDescriptionXml(const std::string &xml,
                         const std::unordered_map<std::string, SymbolPayload> &payloads,
                         std::string &updatedXml,
                         std::string &errorMessage) {
  tinyxml2::XMLDocument doc;
  if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
    errorMessage = "Could not parse description.xml from the GDTF file.";
    return false;
  }

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  if (!fixtureType)
    fixtureType = doc.FirstChildElement("FixtureType");
  if (!fixtureType) {
    errorMessage = "Could not find FixtureType node in description.xml.";
    return false;
  }

  fixtureType->SetAttribute("Editor", "Perastage");

  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  if (!models) {
    errorMessage = "Could not find Models node in description.xml.";
    return false;
  }

  tinyxml2::XMLElement *targetModel = nullptr;
  for (tinyxml2::XMLElement *model = models->FirstChildElement("Model"); model;
       model = model->NextSiblingElement("Model")) {
    const char *name = model->Attribute("Name");
    if (name && std::string(name) == "Main") {
      targetModel = model;
      break;
    }
    if (!targetModel)
      targetModel = model;
  }

  if (!targetModel) {
    errorMessage = "Could not find any Model node in description.xml.";
    return false;
  }

  auto setOffsets = [&](const char *entryPath, const char *xAttr,
                        const char *yAttr) {
    auto it = payloads.find(entryPath);
    if (it == payloads.end())
      return;
    targetModel->SetAttribute(xAttr, it->second.offsetX);
    targetModel->SetAttribute(yAttr, it->second.offsetY);
  };

  setOffsets("models/svg/main.svg", "SVGOffsetX", "SVGOffsetY");
  setOffsets("models/svg_side/main.svg", "SVGSideOffsetX", "SVGSideOffsetY");
  setOffsets("models/svg_front/main.svg", "SVGFrontOffsetX", "SVGFrontOffsetY");

  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  updatedXml = printer.CStr();
  return true;
}

bool RewriteGdtf(const fs::path &sourcePath,
                 const std::unordered_map<std::string, SymbolPayload> &payloads,
                 std::string &errorMessage) {
  wxFileInputStream input(sourcePath.string());
  if (!input.IsOk()) {
    errorMessage = "Could not open fixture GDTF file for reading.";
    return false;
  }

  wxZipInputStream zipInput(input);
  std::unordered_map<std::string, std::string> entries;

  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    const std::string name = entry->GetName().ToStdString();
    if (entry->IsDir())
      continue;

    std::string content;
    ReadAllBytes(zipInput, content);
    entries[name] = std::move(content);
  }

  auto descriptionIt = entries.find("description.xml");
  if (descriptionIt == entries.end()) {
    errorMessage = "The GDTF file does not contain description.xml.";
    return false;
  }

  std::string updatedDescription;
  if (!PatchDescriptionXml(descriptionIt->second, payloads, updatedDescription,
                           errorMessage)) {
    return false;
  }

  entries["description.xml"] = std::move(updatedDescription);
  for (const auto &[path, payload] : payloads)
    entries[path] = payload.svg;

  const fs::path tempPath = sourcePath.string() + ".tmp";
  wxFileOutputStream output(tempPath.string());
  if (!output.IsOk()) {
    errorMessage = "Could not open temporary GDTF file for writing.";
    return false;
  }

  wxZipOutputStream zipOutput(output);
  for (const auto &[name, content] : entries) {
    auto *zipEntry = new wxZipEntry(name);
    zipEntry->SetMethod(wxZIP_METHOD_DEFLATE);
    zipOutput.PutNextEntry(zipEntry);
    zipOutput.Write(content.data(), content.size());
    zipOutput.CloseEntry();
  }
  zipOutput.Close();

  std::error_code ec;
  fs::rename(tempPath, sourcePath, ec);
  if (ec) {
    ec.clear();
    fs::remove(sourcePath, ec);
    ec.clear();
    fs::rename(tempPath, sourcePath, ec);
  }
  if (ec) {
    errorMessage = "Could not replace the original GDTF file.";
    return false;
  }

  return true;
}

} // namespace

bool ApplySymbolsToFixtureGdtf(const std::vector<symbols::Symbol2D> &symbols,
                               const std::string &fixtureUuid,
                               std::string &errorMessage) {
  if (fixtureUuid.empty()) {
    errorMessage = "No fixture was selected for this symbol preview.";
    return false;
  }

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  MvrScene &scene = cfg.GetScene();
  auto fixtureIt = scene.fixtures.find(fixtureUuid);
  if (fixtureIt == scene.fixtures.end()) {
    errorMessage = "Could not resolve the selected fixture in the scene.";
    return false;
  }

  const std::string gdtfPath = ResolveGdtfPath(fixtureIt->second, scene);
  if (gdtfPath.empty()) {
    errorMessage = "Could not resolve the fixture GDTF file path.";
    return false;
  }

  std::unordered_map<std::string, SymbolPayload> payloads;
  SymbolPayload topPayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Top, "models/svg/main.svg",
                         topPayload, errorMessage)) {
    payloads[topPayload.archivePath] = std::move(topPayload);
  }

  SymbolPayload sidePayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Left,
                         "models/svg_side/main.svg", sidePayload,
                         errorMessage)) {
    payloads[sidePayload.archivePath] = std::move(sidePayload);
  }

  SymbolPayload frontPayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Front,
                         "models/svg_front/main.svg", frontPayload,
                         errorMessage)) {
    payloads[frontPayload.archivePath] = std::move(frontPayload);
  }

  if (payloads.empty()) {
    errorMessage =
        "No valid Top, Left, or Front symbol was available to apply to the fixture.";
    return false;
  }

  return RewriteGdtf(fs::path(gdtfPath), payloads, errorMessage);
}

} // namespace symbol_preview
