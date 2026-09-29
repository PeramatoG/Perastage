#include "inspection/inspector_primary_xml_policy.h"

namespace gui::inspection {

// Reports whether a resource is the exact root XML entry shown in the editor.
bool IsInspectorPrimaryXmlResource(
    const perastage::inspection::ResourceDescriptor &resource,
    const std::optional<std::string> &primaryEntry) {
  return resource.kind == perastage::inspection::ResourceKind::XmlText &&
         primaryEntry && resource.displayPath == *primaryEntry;
}

} // namespace gui::inspection
