#include "inspection/inspector_resource_presentation.h"

#include <cassert>

using namespace gui::inspection;
using perastage::inspection::ResourceDescriptor;
using perastage::inspection::ResourceKind;

// Verifies semantic kinds map independently to Source and Preview representations.
int main() {
  ResourceDescriptor resource;
  resource.displayPath = "wheels/gobo.svg";
  resource.kind = ResourceKind::XmlText;
  auto plan = PlanInspectorResourcePresentation(resource, std::nullopt);
  assert(plan.source == InspectorSourceRepresentation::SelectedXml);
  assert(plan.visual == InspectorVisualRepresentation::SvgImage);
  resource.displayPath = "GOBO.SVG";
  assert(PlanInspectorResourcePresentation(resource, std::nullopt).visual ==
         InspectorVisualRepresentation::SvgImage);
  resource.kind = ResourceKind::Text;
  plan = PlanInspectorResourcePresentation(resource, std::nullopt);
  assert(plan.source == InspectorSourceRepresentation::SelectedText);
  assert(plan.visual == InspectorVisualRepresentation::None);
  resource.kind = ResourceKind::XmlText;
  resource.displayPath = "metadata/custom.xml";
  assert(PlanInspectorResourcePresentation(resource, std::nullopt).visual ==
         InspectorVisualRepresentation::None);
  resource.kind = ResourceKind::Image;
  assert(PlanInspectorResourcePresentation(resource, std::nullopt).visual ==
         InspectorVisualRepresentation::RasterImage);
  resource.kind = ResourceKind::Model;
  assert(PlanInspectorResourcePresentation(resource, std::nullopt).visual ==
         InspectorVisualRepresentation::Model);
  resource.kind = ResourceKind::NestedGdtf;
  assert(PlanInspectorResourcePresentation(resource, std::nullopt).visual ==
         InspectorVisualRepresentation::NestedGdtf);
  resource.kind = ResourceKind::Binary;
  assert(!PlanInspectorResourcePresentation(resource, std::nullopt).requiresRead);
  resource.kind = ResourceKind::XmlText;
  resource.normalizedPath = "description.xml";
  resource.displayPath = "description.xml";
  assert(!PlanInspectorResourcePresentation(resource, "description.xml").requiresRead);
  return 0;
}
