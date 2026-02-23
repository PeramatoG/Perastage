#include "windows/symbol_fixture_applier.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "guiconfigservices.h"
#include "gdtfdictionary.h"
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

std::string NormalizeArchivePath(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  while (value.rfind("./", 0) == 0)
    value.erase(0, 2);
  while (!value.empty() && value.front() == '/')
    value.erase(value.begin());
  return value;
}

std::string ToLowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

bool IsDescriptionXmlPath(const std::string &archivePath) {
  const std::string normalized = ToLowerCopy(NormalizeArchivePath(archivePath));
  return normalized == "description.xml" ||
         normalized.size() > sizeof("/description.xml") - 1 &&
             normalized.rfind("/description.xml") ==
                 normalized.size() - (sizeof("/description.xml") - 1);
}

std::string BuildDescriptionMissingMessage(
    const fs::path &gdtfPath,
    const std::vector<std::string> &sampleEntries,
    bool foundCaseInsensitiveVariant) {
  std::ostringstream message;
  message << "Could not locate description.xml in GDTF archive: "
          << gdtfPath.string() << ".";
  if (foundCaseInsensitiveVariant) {
    message
        << " A case-insensitive match was found, but this loader requires the canonical "
           "name/path (description.xml).";
  }
  if (!sampleEntries.empty()) {
    message << " Sample archive entries: ";
    for (size_t i = 0; i < sampleEntries.size(); ++i) {
      if (i > 0)
        message << ", ";
      message << sampleEntries[i];
    }
    message << ".";
  }
  return message.str();
}


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

std::string ResolveSceneGdtfPath(const Fixture &fixture, const MvrScene &scene) {
  const fs::path specPath = fs::path(fixture.gdtfSpec);
  if (specPath.empty())
    return {};

  std::error_code ec;
  if (specPath.is_absolute() && fs::exists(specPath, ec) && !ec)
    return specPath.string();

  if (!scene.basePath.empty()) {
    ec.clear();
    const fs::path fromScene = fs::path(scene.basePath) / specPath;
    if (fs::exists(fromScene, ec) && !ec)
      return fromScene.string();
  }

  ec.clear();
  const fs::path fromCwd = fs::current_path(ec) / specPath;
  if (!ec && fs::exists(fromCwd, ec) && !ec)
    return fromCwd.string();

  return {};
}

std::string ResolveLibraryGdtfPath(const Fixture &fixture) {
  const fs::path specPath = fs::path(fixture.gdtfSpec);
  const std::string specFileName = specPath.filename().string();

  std::error_code ec;
  const fs::path fixturesLibrary =
      fs::path(ProjectUtils::GetDefaultLibraryPath("fixtures"));

  auto dictOpt = GdtfDictionary::Load();
  if (dictOpt) {
    const auto &dict = *dictOpt;
    auto findByKey = [&](const std::string &key) -> std::string {
      if (key.empty())
        return {};
      auto it = dict.find(key);
      if (it == dict.end() || it->second.path.empty())
        return {};
      std::error_code localEc;
      if (!fs::exists(it->second.path, localEc) || localEc)
        return {};
      return it->second.path;
    };

    if (std::string byType = findByKey(fixture.typeName); !byType.empty())
      return byType;

    if (!specFileName.empty()) {
      for (const auto &[type, entry] : dict) {
        (void)type;
        if (entry.path.empty())
          continue;

        const fs::path entryPath = fs::path(entry.path);
        if (entryPath.filename() != specFileName)
          continue;

        std::error_code localEc;
        if (fs::exists(entryPath, localEc) && !localEc)
          return entryPath.string();
      }
    }
  }

  if (!specFileName.empty()) {
    ec.clear();
    const fs::path exactPath = fixturesLibrary / specFileName;
    if (fs::exists(exactPath, ec) && !ec)
      return exactPath.string();
  }

  return {};
}


std::string ResolveModelSvgBasename(const fs::path &gdtfPath,
                                    std::string &errorMessage) {
  wxFileInputStream input(gdtfPath.string());
  if (!input.IsOk()) {
    errorMessage = "Could not open fixture GDTF file for reading.";
    return {};
  }

  wxZipInputStream zipInput(input);
  std::unique_ptr<wxZipEntry> entry;
  std::string descriptionXml;
  std::vector<std::string> sampleEntries;
  bool foundCaseInsensitiveVariant = false;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const std::string entryName = entry->GetName().ToStdString();
    const std::string normalizedEntry = NormalizeArchivePath(entryName);
    if (sampleEntries.size() < 5)
      sampleEntries.push_back(normalizedEntry);

    if (ToLowerCopy(normalizedEntry) == "description.xml")
      foundCaseInsensitiveVariant = true;

    if (!IsDescriptionXmlPath(normalizedEntry))
      continue;
    if (!ReadAllBytes(zipInput, descriptionXml)) {
      errorMessage = "Could not read description.xml from the GDTF file.";
      return {};
    }
    break;
  }

  if (descriptionXml.empty()) {
    errorMessage = BuildDescriptionMissingMessage(gdtfPath, sampleEntries,
                                                  foundCaseInsensitiveVariant);
    return {};
  }

  tinyxml2::XMLDocument doc;
  if (doc.Parse(descriptionXml.c_str(), descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS) {
    errorMessage = "Could not parse description.xml from the GDTF file.";
    return {};
  }

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  if (!fixtureType)
    fixtureType = doc.FirstChildElement("FixtureType");
  if (!fixtureType) {
    errorMessage = "Could not find FixtureType node in description.xml.";
    return {};
  }

  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  if (!models) {
    errorMessage = "Could not find Models node in description.xml.";
    return {};
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
    return {};
  }

  const char *fileAttr = targetModel->Attribute("File");
  if (fileAttr && std::string(fileAttr).size() > 0)
    return std::string(fileAttr);

  const char *nameAttr = targetModel->Attribute("Name");
  if (nameAttr && std::string(nameAttr).size() > 0)
    return std::string(nameAttr);

  return "main";
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

  out.archivePath = NormalizeArchivePath(archivePath);
  out.offsetX = -symbol->bounds.min.x;
  out.offsetY = -symbol->bounds.min.y;
  if (!symbol_preview::ExportSymbolToSvgString(*symbol, out.svg, errorMessage))
    return false;

  return true;
}

bool PatchDescriptionXml(const std::string &xml,
                         const std::unordered_map<std::string, SymbolPayload> &payloads,
                         const std::string &topPath,
                         const std::string &sidePath,
                         const std::string &frontPath,
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

  setOffsets(topPath.c_str(), "SVGOffsetX", "SVGOffsetY");
  setOffsets(sidePath.c_str(), "SVGSideOffsetX", "SVGSideOffsetY");
  setOffsets(frontPath.c_str(), "SVGFrontOffsetX", "SVGFrontOffsetY");

  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  updatedXml = printer.CStr();
  return true;
}

bool VerifyArchiveEntries(const fs::path &archivePath,
                        const std::unordered_map<std::string, SymbolPayload> &payloads) {
  wxFileInputStream input(archivePath.string());
  if (!input.IsOk())
    return false;

  wxZipInputStream zipInput(input);
  std::unordered_map<std::string, bool> found;
  for (const auto &[path, payload] : payloads) {
    (void)payload;
    found[NormalizeArchivePath(path)] = false;
  }

  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    const std::string name = NormalizeArchivePath(entry->GetName().ToStdString());
    auto it = found.find(name);
    if (it != found.end())
      it->second = true;
  }

  for (const auto &[path, wasFound] : found) {
    (void)path;
    if (!wasFound)
      return false;
  }
  return true;
}

bool RewriteGdtf(const fs::path &sourcePath,
                 const std::unordered_map<std::string, SymbolPayload> &payloads,
                 const std::string &topPath,
                 const std::string &sidePath,
                 const std::string &frontPath,
                 std::string &errorMessage) {
  std::vector<std::pair<std::string, std::string>> entries;
  std::vector<std::string> sampleEntries;
  bool foundCaseInsensitiveVariant = false;
  {
    wxFileInputStream input(sourcePath.string());
    if (!input.IsOk()) {
      errorMessage = "Could not open fixture GDTF file for reading.";
      return false;
    }

    wxZipInputStream zipInput(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zipInput.GetNextEntry())), entry) {
      const std::string name = entry->GetName().ToStdString();
      if (entry->IsDir())
        continue;

      const std::string normalizedName = NormalizeArchivePath(name);
      if (sampleEntries.size() < 5)
        sampleEntries.push_back(normalizedName);
      if (ToLowerCopy(normalizedName) == "description.xml")
        foundCaseInsensitiveVariant = true;

      std::string content;
      ReadAllBytes(zipInput, content);
      entries.emplace_back(name, std::move(content));
    }
  }

  auto descriptionIt = std::find_if(entries.begin(), entries.end(),
                                    [](const auto &entry) {
                                      return IsDescriptionXmlPath(entry.first);
                                    });
  if (descriptionIt == entries.end()) {
    errorMessage = BuildDescriptionMissingMessage(sourcePath, sampleEntries,
                                                  foundCaseInsensitiveVariant);
    return false;
  }

  std::string updatedDescription;
  if (!PatchDescriptionXml(descriptionIt->second, payloads, topPath, sidePath,
                           frontPath, updatedDescription, errorMessage)) {
    return false;
  }

  descriptionIt->second = std::move(updatedDescription);
  for (const auto &[path, payload] : payloads) {
    const std::string normalizedPath = NormalizeArchivePath(path);
    auto existing = std::find_if(entries.begin(), entries.end(),
                                 [&](const auto &entry) {
                                   return NormalizeArchivePath(entry.first) ==
                                          normalizedPath;
                                 });
    if (existing != entries.end())
      existing->second = payload.svg;
    else
      entries.emplace_back(normalizedPath, payload.svg);
  }

  const fs::path tempPath = sourcePath.string() + ".tmp";
  {
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
  }

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

  if (!VerifyArchiveEntries(sourcePath, payloads)) {
    errorMessage = "GDTF was saved but generated SVG views were not found in archive.";
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
  const auto &fixtures = cfg.GetScene().fixtures;
  auto fixtureIt = fixtures.find(fixtureUuid);
  if (fixtureIt == fixtures.end()) {
    errorMessage = "Could not resolve the selected fixture in the scene.";
    return false;
  }

  const MvrScene &scene = cfg.GetScene();
  std::vector<fs::path> targetPaths;
  std::unordered_set<std::string> uniqueTargets;

  const std::string scenePath = ResolveSceneGdtfPath(fixtureIt->second, scene);
  if (!scenePath.empty() && uniqueTargets.insert(scenePath).second)
    targetPaths.emplace_back(scenePath);

  const std::string libraryPath = ResolveLibraryGdtfPath(fixtureIt->second);
  if (!libraryPath.empty() && uniqueTargets.insert(libraryPath).second)
    targetPaths.emplace_back(libraryPath);

  if (targetPaths.empty()) {
    errorMessage = "Could not resolve fixture GDTF path in scene or fixtures library.";
    return false;
  }

  std::string modelSvgBase = ResolveModelSvgBasename(targetPaths.front(), errorMessage);
  if (modelSvgBase.empty())
    return false;

  const std::string topSvgPath = "models/svg/" + modelSvgBase + ".svg";
  const std::string sideSvgPath = "models/svg_side/" + modelSvgBase + ".svg";
  const std::string frontSvgPath = "models/svg_front/" + modelSvgBase + ".svg";
  const std::string bottomSvgPath = "models/svg/" + modelSvgBase + "_bottom.svg";

  std::unordered_map<std::string, SymbolPayload> payloads;
  SymbolPayload topPayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Top, topSvgPath,
                         topPayload, errorMessage)) {
    payloads[topPayload.archivePath] = std::move(topPayload);
  }

  SymbolPayload sidePayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Left, sideSvgPath,
                         sidePayload, errorMessage)) {
    payloads[sidePayload.archivePath] = std::move(sidePayload);
  }

  SymbolPayload frontPayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Front, frontSvgPath,
                         frontPayload, errorMessage)) {
    payloads[frontPayload.archivePath] = std::move(frontPayload);
  }

  SymbolPayload bottomPayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Bottom, bottomSvgPath,
                         bottomPayload, errorMessage)) {
    payloads[bottomPayload.archivePath] = std::move(bottomPayload);
  }

  if (payloads.empty()) {
    errorMessage =
        "No valid symbol views were available to apply to the fixture.";
    return false;
  }

  for (const auto &targetPath : targetPaths) {
    if (!RewriteGdtf(targetPath, payloads, topSvgPath, sideSvgPath, frontSvgPath,
                     errorMessage))
      return false;
  }

  return true;
}

} // namespace symbol_preview
