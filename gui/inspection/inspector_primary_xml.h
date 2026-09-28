#pragma once

#include "inspection/inspector_source_context.h"
#include "inspection/inspector_primary_xml_policy.h"

#include <optional>
#include <string>

namespace gui::inspection {

// Returns the authoritative root XML entry already shown for one source.
std::optional<std::string>
InspectorPrimaryXmlEntry(const DisplayedPackageContext &context);

} // namespace gui::inspection
