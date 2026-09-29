#pragma once

#include "inspection/resource_inspection.h"

#include <optional>
#include <string>

namespace gui::inspection {

// Reports whether a resource is the exact root XML entry shown in the editor.
bool IsInspectorPrimaryXmlResource(
    const perastage::inspection::ResourceDescriptor &resource,
    const std::optional<std::string> &primaryEntry);

} // namespace gui::inspection
