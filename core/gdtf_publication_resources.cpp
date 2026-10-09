#include "gdtf_publication_resources.h"

#include "gdtf_archive_reader.h"
#include "gdtf_fixture_type_vocabulary.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <set>
#include <string_view>

namespace fixture_gdtf {
namespace {

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool RemovePrivateSvgAttributes(tinyxml2::XMLElement *element) {
  bool changed = false;
  for (const auto *attribute = element->FirstAttribute(); attribute;) {
    const std::string name = attribute->Name();
    attribute = attribute->Next();
    if (Lower(name).rfind("data-perastage-", 0) == 0) {
      element->DeleteAttribute(name.c_str());
      changed = true;
    }
  }
  for (auto *child = element->FirstChildElement(); child;
       child = child->NextSiblingElement())
    changed = RemovePrivateSvgAttributes(child) || changed;
  return changed;
}

bool HasLegacyBottomRevision(const tinyxml2::XMLElement *fixture) {
  const auto *revisions = fixture ? fixture->FirstChildElement("Revisions") : nullptr;
  constexpr std::array<std::string_view, 4> labels = {"top", "side", "front", "bottom"};
  for (const auto *revision = revisions ? revisions->FirstChildElement("Revision") : nullptr;
       revision; revision = revision->NextSiblingElement("Revision")) {
    const char *modifiedBy = revision->Attribute("ModifiedBy");
    const char *text = revision->Attribute("Text");
    if (!modifiedBy || !text || !gdtf::IsLegacyPerastageEditorValue(modifiedBy))
      continue;
    for (unsigned mask = 8; mask < 16; ++mask) {
      std::string expected = "Applied fixture SVG symbol views (";
      bool first = true;
      for (unsigned index = 0; index < labels.size(); ++index) {
        if (!(mask & (1u << index)))
          continue;
        if (!first)
          expected += ", ";
        expected += labels[index];
        first = false;
      }
      expected += ')';
      if (expected == text)
        return true;
    }
  }
  return false;
}

} // namespace

bool BuildStandardPublicationResourceMutations(
    const std::filesystem::path &sourcePath,
    std::vector<GdtfCanonicalizer::ResourceMutation> &mutations,
    std::string &errorMessage) {
  mutations.clear();
  const auto archive = gdtf::ReadGdtfArchive(sourcePath);
  if (!archive.Success()) {
    errorMessage = "Could not inspect the derivative publication source.";
    return false;
  }
  tinyxml2::XMLDocument description;
  if (description.Parse(archive.descriptionXml.c_str(), archive.descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS) {
    errorMessage = "Could not parse the derivative publication description.";
    return false;
  }
  const auto *root = description.FirstChildElement("GDTF");
  const auto *fixture = root ? root->FirstChildElement("FixtureType") : nullptr;
  const auto *models = fixture ? fixture->FirstChildElement("Models") : nullptr;
  std::set<std::string> standardPaths;
  std::set<std::string> historicalBottomPaths;
  for (const auto *model = models ? models->FirstChildElement("Model") : nullptr;
       model; model = model->NextSiblingElement("Model")) {
    const char *file = model->Attribute("File");
    if (!file || !*file)
      continue;
    for (const auto *directory : {"models/svg/", "models/svg_side/", "models/svg_front/"})
      standardPaths.insert(Lower(std::string(directory) + file + ".svg"));
    historicalBottomPaths.insert(Lower("models/svg/" + std::string(file) + "_bottom.svg"));
    historicalBottomPaths.insert(Lower("models/svg_bottom/" + std::string(file) + ".svg"));
  }
  const bool legacyBottomRevision = HasLegacyBottomRevision(fixture);
  for (const auto &entry : archive.entries) {
    if (entry.directory)
      continue;
    const std::string lower = Lower(entry.path);
    if (lower.rfind("perastage/", 0) == 0) {
      mutations.push_back({entry.path, std::nullopt});
      continue;
    }
    const bool standard = standardPaths.contains(lower);
    const bool unsupportedBottom = !standard &&
        (lower.rfind("models/svg_bottom/", 0) == 0 ||
         historicalBottomPaths.contains(lower));
    if (unsupportedBottom && legacyBottomRevision &&
        historicalBottomPaths.contains(lower)) {
      mutations.push_back({entry.path, std::nullopt});
      continue;
    }
    if (!lower.ends_with(".svg")) {
      if (unsupportedBottom) {
        errorMessage = "Unknown unsupported Bottom resources require explicit repair before publication.";
        return false;
      }
      continue;
    }
    const auto payload = gdtf::ReadGdtfArchiveResource(sourcePath, entry.path);
    if (!payload.Success() || payload.filesystemFallback ||
        payload.entryPath != entry.path) {
      if (unsupportedBottom) {
        errorMessage = "Unknown unsupported Bottom resources require explicit repair before publication.";
        return false;
      }
      continue;
    }
    const std::string bytes(payload.bytes.begin(), payload.bytes.end());
    tinyxml2::XMLDocument svg;
    if (svg.Parse(bytes.c_str(), bytes.size()) != tinyxml2::XML_SUCCESS) {
      if (Lower(bytes).find("data-perastage-") != std::string::npos) {
        errorMessage = "Malformed legacy SVG metadata requires explicit repair.";
        return false;
      }
      if (unsupportedBottom) {
        errorMessage = "Unknown unsupported Bottom resources require explicit repair before publication.";
        return false;
      }
      continue;
    }
    auto *element = svg.FirstChildElement("svg");
    const bool marked = element && RemovePrivateSvgAttributes(element);
    if (!marked) {
      if (unsupportedBottom) {
        errorMessage = "Unknown unsupported Bottom resources require explicit repair before publication.";
        return false;
      }
      continue;
    }
    if (!standard && (unsupportedBottom || lower.ends_with("_bottom.svg"))) {
      mutations.push_back({entry.path, std::nullopt});
    } else {
      tinyxml2::XMLPrinter printer;
      svg.Print(&printer);
      mutations.push_back({entry.path, std::string(printer.CStr())});
    }
  }
  errorMessage.clear();
  return true;
}

} // namespace fixture_gdtf
