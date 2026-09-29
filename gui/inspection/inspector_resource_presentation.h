#pragma once

#include "inspection/resource_inspection.h"

#include <optional>
#include <string>

namespace gui::inspection {

enum class InspectorSourceRepresentation { Primary, SelectedXml, SelectedText };
enum class InspectorVisualRepresentation { None, RasterImage, SvgImage, Model, NestedGdtf };

struct InspectorResourcePresentationPlan final {
  InspectorSourceRepresentation source = InspectorSourceRepresentation::Primary;
  InspectorVisualRepresentation visual = InspectorVisualRepresentation::None;
  bool requiresRead = false;
};

// Maps semantic Core facts to independent textual and visual representations.
InspectorResourcePresentationPlan PlanInspectorResourcePresentation(
    const perastage::inspection::ResourceDescriptor &resource,
    const std::optional<std::string> &primaryXmlEntry);

} // namespace gui::inspection
