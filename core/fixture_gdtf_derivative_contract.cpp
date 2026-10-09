#include "fixture_gdtf_derivative_contract.h"

#include "filesystem_path_utils.h"
#include "gdtf_archive_reader.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <tinyxml2.h>

namespace fixture_gdtf {
namespace {

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool ContainsPrivateSvgMetadata(const tinyxml2::XMLElement *element) {
  if (Lower(element->Name()).rfind("perastage", 0) == 0)
    return true;
  for (const auto *attribute = element->FirstAttribute(); attribute;
       attribute = attribute->Next()) {
    if (Lower(attribute->Name()).rfind("data-perastage-", 0) == 0)
      return true;
  }
  for (const auto *child = element->FirstChildElement(); child;
       child = child->NextSiblingElement())
    if (ContainsPrivateSvgMetadata(child))
      return true;
  return false;
}

} // namespace

bool ValidatePublishedDerivative(const std::string &path,
                                 std::string &errorMessage) {
  const auto archive = gdtf::ReadGdtfArchive(PathUtils::PathFromUtf8(path));
  if (!archive.Success()) {
    errorMessage = "Could not read the fixture derivative archive.";
    return false;
  }
  tinyxml2::XMLDocument document;
  if (document.Parse(archive.descriptionXml.c_str(), archive.descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS) {
    errorMessage = "The fixture derivative description is malformed.";
    return false;
  }
  const auto *root = document.FirstChildElement("GDTF");
  const auto *fixture = root ? root->FirstChildElement("FixtureType") : nullptr;
  if (!fixture || !fixture->FirstChildElement("AttributeDefinitions") ||
      !fixture->FirstChildElement("Geometries") ||
      !fixture->FirstChildElement("DMXModes")) {
    errorMessage = "The fixture derivative is missing required definition sections.";
    return false;
  }
  if (ContainsPrivateSvgMetadata(root)) {
    errorMessage = "Private Perastage XML metadata cannot be published in a GDTF.";
    return false;
  }
  for (const auto &entry : archive.entries) {
    const std::string lower = Lower(entry.path);
    if (lower.rfind("perastage/", 0) == 0 ||
        lower.rfind("models/svg_bottom/", 0) == 0) {
      errorMessage = "Private Perastage resources cannot be published in a GDTF.";
      return false;
    }
    if (entry.directory || !lower.ends_with(".svg"))
      continue;
    const auto resource = gdtf::ReadGdtfArchiveResource(
        archive.sourcePath, entry.path);
    if (!resource.Success() || resource.filesystemFallback ||
        resource.entryPath != entry.path)
      continue;
    const std::string bytes(resource.bytes.begin(), resource.bytes.end());
    tinyxml2::XMLDocument svg;
    if (svg.Parse(bytes.c_str(), bytes.size()) != tinyxml2::XML_SUCCESS) {
      if (Lower(bytes).find("data-perastage-") != std::string::npos) {
        errorMessage = "Malformed legacy SVG metadata requires explicit repair.";
        return false;
      }
      continue;
    }
    if (const auto *element = svg.FirstChildElement("svg");
        element && ContainsPrivateSvgMetadata(element)) {
      errorMessage = "Private Perastage SVG metadata cannot be published in a GDTF.";
      return false;
    }
  }
  errorMessage.clear();
  return true;
}

} // namespace fixture_gdtf
