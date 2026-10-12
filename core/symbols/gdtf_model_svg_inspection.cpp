#include "symbols/gdtf_model_svg_inspection.h"

#include "gdtf_archive_reader.h"
#include "symbols/fixture_symbol_resource_contract.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tinyxml2.h>

namespace symbols {
namespace {

std::string Attribute(const tinyxml2::XMLElement *element, const char *name) {
  const char *value = element ? element->Attribute(name) : nullptr;
  return value ? value : "";
}

double Number(const tinyxml2::XMLElement *element, const char *name,
              std::vector<std::string> &diagnostics) {
  double value = 0.0;
  if (element->Attribute(name) &&
      (element->QueryDoubleAttribute(name, &value) != tinyxml2::XML_SUCCESS ||
       !std::isfinite(value))) {
    diagnostics.push_back(std::string(name) + " is not a finite number.");
    return 0.0;
  }
  return value;
}

void GeometryReferences(const tinyxml2::XMLElement *parent,
                        const std::string &path,
                        std::vector<GdtfModelSvgInspection> &models) {
  for (auto *element = parent ? parent->FirstChildElement() : nullptr;
       element; element = element->NextSiblingElement()) {
    const std::string name = Attribute(element, "Name");
    const std::string current = path + "/" + (name.empty() ? element->Name() : name);
    const std::string modelName = Attribute(element, "Model");
    if (!modelName.empty())
      for (auto &model : models)
        if (model.name == modelName)
          model.geometryReferences.push_back(current);
    GeometryReferences(element, current, models);
  }
}

ModelSvgResourceInspection InspectView(
    const std::filesystem::path &source, const gdtf::ArchiveReadResult &archive,
    const ModelSvgViewDefinition &definition, const std::string &file,
    const tinyxml2::XMLElement *model, std::vector<std::string> &diagnostics) {
  ModelSvgResourceInspection resource;
  resource.view = definition.view;
  resource.offsetX = Number(model, definition.offsetXAttribute.c_str(), diagnostics);
  resource.offsetY = Number(model, definition.offsetYAttribute.c_str(), diagnostics);
  if (!IsSafeGdtfModelResourceBasename(file)) {
    resource.diagnostic = "No safe Model/@File resource basename.";
    return resource;
  }
  resource.archivePath = definition.directory + "/" + file + ".svg";
  const auto count = std::count_if(archive.entries.begin(), archive.entries.end(),
      [&](const auto &entry) { return !entry.directory && entry.path == resource.archivePath; });
  resource.present = count > 0;
  if (!resource.present)
    return resource;
  if (count != 1) {
    resource.state = ModelSvgResourceState::Ambiguous;
    resource.diagnostic = "Duplicate exact archive resource path.";
    return resource;
  }
  const auto payload = gdtf::ReadGdtfArchiveResource(source, resource.archivePath);
  if (!payload.Success() || payload.filesystemFallback ||
      payload.entryPath != resource.archivePath) {
    resource.state = ModelSvgResourceState::Unreadable;
    resource.diagnostic = "Present archive resource could not be read safely.";
    return resource;
  }
  tinyxml2::XMLDocument svg;
  const std::string text(payload.bytes.begin(), payload.bytes.end());
  const bool parsed = svg.Parse(text.c_str(), text.size()) == tinyxml2::XML_SUCCESS;
  const std::string rootName = svg.RootElement() ? svg.RootElement()->Name() : "";
  if (!parsed || (rootName != "svg" && !rootName.ends_with(":svg"))) {
    resource.state = ModelSvgResourceState::Malformed;
    resource.diagnostic = "Resource is not readable SVG XML.";
    return resource;
  }
  // Presence and XML readability do not assert full SVG conformance or preview support.
  resource.state = ModelSvgResourceState::Present;
  const char *legacyVersion = svg.RootElement()->Attribute(kPerastageSymbolVersionAttribute);
  resource.legacyPerastage = legacyVersion &&
      std::string(legacyVersion) == std::to_string(kCurrentPerastageSymbolResourceVersion);
  if (resource.legacyPerastage)
    resource.diagnostic = "Recognized legacy Perastage symbol at an official Model SVG path; original authored bytes cannot be inferred.";
  return resource;
}

} // namespace

std::vector<ModelSvgViewDefinition> GdtfModelSvgViews(const std::string &version) {
  if (version == "1.1")
    return {{SymbolViewKind::Top, "models/svg", "SVGOffsetX", "SVGOffsetY"}};
  if (version == "1.2")
    return {{SymbolViewKind::Top, "models/svg", "SVGOffsetX", "SVGOffsetY"},
            {SymbolViewKind::Front, "models/svg_front", "SVGFrontOffsetX", "SVGFrontOffsetY"},
            {SymbolViewKind::Left, "models/svg_side", "SVGSideOffsetX", "SVGSideOffsetY"}};
  return {};
}

bool IsSafeGdtfModelResourceBasename(const std::string &file) {
  return !file.empty() && file != "." && file != ".." &&
      file.find_first_of("/\\:\0", 0, 4) == std::string::npos;
}

GdtfModelSvgCatalog InspectGdtfModelSvgs(const std::filesystem::path &source) {
  GdtfModelSvgCatalog result;
  const auto archive = gdtf::ReadGdtfArchive(source);
  if (!archive.Success()) {
    result.diagnostics.push_back("Could not read the GDTF archive.");
    return result;
  }
  tinyxml2::XMLDocument document;
  if (document.Parse(archive.descriptionXml.c_str(), archive.descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS) {
    result.diagnostics.push_back("The GDTF description is malformed.");
    return result;
  }
  const auto *root = document.FirstChildElement("GDTF");
  const auto *fixture = root ? root->FirstChildElement("FixtureType") : nullptr;
  if (!fixture) {
    result.diagnostics.push_back("No GDTF FixtureType.");
    return result;
  }
  result.dataVersion = Attribute(root, "DataVersion");
  const auto definitions = GdtfModelSvgViews(result.dataVersion);
  if (definitions.empty())
    result.diagnostics.push_back("No published Model SVG view set supported for this DataVersion.");
  const auto *models = fixture->FirstChildElement("Models");
  std::map<std::string, std::size_t> files;
  for (auto *model = models ? models->FirstChildElement("Model") : nullptr;
       model; model = model->NextSiblingElement("Model")) {
    GdtfModelSvgInspection inspected;
    inspected.name = Attribute(model, "Name");
    inspected.file = Attribute(model, "File");
    inspected.primitiveType = Attribute(model, "PrimitiveType");
    inspected.lengthMeters = Number(model, "Length", inspected.diagnostics);
    inspected.widthMeters = Number(model, "Width", inspected.diagnostics);
    inspected.heightMeters = Number(model, "Height", inspected.diagnostics);
    if (!IsSafeGdtfModelResourceBasename(inspected.file))
      inspected.diagnostics.push_back("Model/@File is absent or unsafe; no resource basename inferred.");
    else
      ++files[inspected.file];
    for (const auto &definition : definitions)
      inspected.views.push_back(InspectView(source, archive, definition,
          inspected.file, model, inspected.diagnostics));
    result.models.push_back(std::move(inspected));
  }
  for (auto &model : result.models) {
    model.ambiguousFile = !model.file.empty() && files[model.file] > 1;
    if (model.ambiguousFile)
      model.diagnostics.push_back("Model/@File is shared by multiple Models; mutation is ambiguous.");
  }
  GeometryReferences(fixture->FirstChildElement("Geometries"), "", result.models);
  result.success = true;
  return result;
}

} // namespace symbols
