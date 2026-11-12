#pragma once

#include "mvrscene.h"
#include "mvr_export_diagnostic.h"
#include <functional>
#include <unordered_set>

namespace tinyxml2 { class XMLDocument; }
namespace mvr {
// Source standard subtrees stay immutable evidence until the canonical XML
// writer projects them. They are never executed or installed into live objects.
bool CanProjectStandardSourceField(const MvrPreservationIssue &issue);
struct StandardFieldProjectionResult {
  std::vector<MvrExportDiagnostic> diagnostics;
  std::unordered_set<std::string> resources;
};
// The resource callback uses the existing archive registry, preserving its path,
// dependency and missing-resource checks. The boolean selects a GDTF binding.
using StandardFieldResource = std::function<std::string(
    const std::string &owner, const std::string &source, bool gdtf)>;
StandardFieldProjectionResult ProjectStandardSourceFields(
    tinyxml2::XMLDocument &document, const MvrScene &scene,
    const StandardFieldResource &registerResource);
// Merge is an explicit mutation boundary: retained references and resource
// paths must follow the same collision remaps as modeled objects.
void ReconcileStandardSourceFields(MvrScene &scene,
    const std::function<std::string(const std::string &)> &mapUuid,
    const std::function<std::string(const std::string &)> &mapResource);
} // namespace mvr
