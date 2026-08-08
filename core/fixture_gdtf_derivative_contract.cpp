#include "fixture_gdtf_derivative_contract.h"

#include "wx_path_utils.h"

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <tinyxml2.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <set>
#include <string>

namespace fixture_gdtf {
namespace {

// Normalizes a GDTF archive path for contract comparisons.
std::string NormalizePath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.rfind("./", 0) == 0)
    path.erase(0, 2);
  std::transform(path.begin(), path.end(), path.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });
  return path;
}

// Resolves the symbol model basename from a GDTF description.
std::string ResolveSymbolBase(const std::string &description) {
  tinyxml2::XMLDocument document;
  if (document.Parse(description.c_str(), description.size()) !=
      tinyxml2::XML_SUCCESS)
    return {};
  tinyxml2::XMLElement *fixtureType = document.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  tinyxml2::XMLElement *models =
      fixtureType ? fixtureType->FirstChildElement("Models") : nullptr;
  tinyxml2::XMLElement *selected = nullptr;
  for (tinyxml2::XMLElement *model =
           models ? models->FirstChildElement("Model") : nullptr;
       model; model = model->NextSiblingElement("Model")) {
    if (!selected)
      selected = model;
    const char *name = model->Attribute("Name");
    if (name && std::string(name) == "Main") {
      selected = model;
      break;
    }
  }
  if (!selected)
    return {};
  const char *file = selected->Attribute("File");
  const char *name = selected->Attribute("Name");
  return file && *file ? file : (name && *name ? name : "main");
}

} // namespace

// Validates the four-view contract required by a published Perastage derivative.
bool ValidatePublishedDerivative(const std::string &path,
                                 std::string &errorMessage) {
  wxFileInputStream input(
      WxPathUtils::WxStringFromFilesystemPath(std::filesystem::path(path)));
  if (!input.IsOk()) {
    errorMessage = "Could not open the fixture derivative.";
    return false;
  }
  wxZipInputStream zip(input);
  std::set<std::string> entries;
  std::string description;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const std::string normalized =
        NormalizePath(entry->GetName().ToStdString());
    entries.insert(normalized);
    if (normalized == "description.xml") {
      char buffer[8192];
      while (true) {
        zip.Read(buffer, sizeof(buffer));
        const size_t count = zip.LastRead();
        if (!count)
          break;
        description.append(buffer, count);
      }
    }
  }
  const std::string base = ResolveSymbolBase(description);
  if (base.empty()) {
    errorMessage = "Could not resolve the fixture derivative symbol model.";
    return false;
  }
  const std::set<std::string> required = {
      NormalizePath("models/svg/" + base + ".svg"),
      NormalizePath("models/svg/" + base + "_bottom.svg"),
      NormalizePath("models/svg_front/" + base + ".svg"),
      NormalizePath("models/svg_side/" + base + ".svg")};
  for (const std::string &requiredPath : required) {
    if (!entries.contains(requiredPath)) {
      errorMessage = "Fixture derivative is missing required symbol view '" +
                     requiredPath + "'.";
      return false;
    }
  }
  errorMessage.clear();
  return true;
}

} // namespace fixture_gdtf
