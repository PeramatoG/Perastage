#include "inspection/inspector_resource_presentation.h"

#include "inspection/inspector_primary_xml_policy.h"

#include <algorithm>
#include <cctype>

namespace gui::inspection {
namespace {

// Detects SVG spelling only after Core has established XML semantics.
bool HasSvgExtension(const std::string &path) {
  const auto dot = path.find_last_of('.');
  if (dot == std::string::npos)
    return false;
  std::string extension = path.substr(dot);
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
  return extension == ".svg";
}

} // namespace

// Maps semantic Core facts to independent textual and visual representations.
InspectorResourcePresentationPlan PlanInspectorResourcePresentation(
    const perastage::inspection::ResourceDescriptor &resource,
    const std::optional<std::string> &primaryXmlEntry) {
  if (primaryXmlEntry && IsInspectorPrimaryXmlResource(resource, primaryXmlEntry))
    return {};
  switch (resource.kind) {
  case perastage::inspection::ResourceKind::XmlText:
    return {InspectorSourceRepresentation::SelectedXml,
            HasSvgExtension(resource.displayPath)
                ? InspectorVisualRepresentation::SvgImage
                : InspectorVisualRepresentation::None,
            true};
  case perastage::inspection::ResourceKind::Text:
    return {InspectorSourceRepresentation::SelectedText,
            InspectorVisualRepresentation::None, true};
  case perastage::inspection::ResourceKind::Image:
    return {InspectorSourceRepresentation::Primary,
            InspectorVisualRepresentation::RasterImage, true};
  case perastage::inspection::ResourceKind::Model:
    return {InspectorSourceRepresentation::Primary,
            InspectorVisualRepresentation::Model, true};
  case perastage::inspection::ResourceKind::NestedGdtf:
    return {InspectorSourceRepresentation::Primary,
            InspectorVisualRepresentation::NestedGdtf, true};
  case perastage::inspection::ResourceKind::Binary:
    return {};
  }
  return {};
}

} // namespace gui::inspection
