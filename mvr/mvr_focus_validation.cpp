/* This file is part of Perastage. */
#include "mvr_standard_preservation.h"
#include "uuidutils.h"

#include <tinyxml2.h>
#include <unordered_set>

namespace mvr {
std::vector<MvrExportDiagnostic> CheckSerializedFocus(tinyxml2::XMLElement &scene) {
  std::vector<MvrExportDiagnostic> result;
  std::unordered_set<std::string> focusUuids;
  std::vector<std::pair<std::string, std::string>> references;
  auto error = [&](const std::string &uuid, const std::string &detail) {
    result.push_back({MvrExportDiagnosticCode::StructuralValidationFailed,
        MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
        true, "FocusPoint", {}, uuid, {}, detail});
  };
  // Traverse scene ownership only; foreign UserData may use arbitrary names.
  auto visit = [&](auto &&self, tinyxml2::XMLElement &container) -> void {
    for (auto *node = container.FirstChildElement(); node;
         node = node->NextSiblingElement()) {
      const std::string kind = node->Name();
      const std::string uuid = node->Attribute("uuid") ? node->Attribute("uuid") : "";
      if (kind == "FocusPoint") {
        if (CanonicalizeUuid(uuid) != uuid || uuid.empty() ||
            uuid == "00000000-0000-0000-0000-000000000000" ||
            !focusUuids.insert(uuid).second)
          error(uuid, "Invalid or duplicate FocusPoint UUID.");
        if (!node->FirstChildElement("Geometries") || node->FirstChildElement("ChildList"))
          error(uuid, "FocusPoint requires Geometries and cannot own ChildList.");
      }
      if (kind == "Fixture")
        if (auto *focus = node->FirstChildElement("Focus"))
          references.emplace_back(uuid, focus->GetText() ? focus->GetText() : "");
      if (auto *children = node->FirstChildElement("ChildList")) self(self, *children);
    }
  };
  if (auto *layers = scene.FirstChildElement("Layers")) visit(visit, *layers);
  for (const auto &[uuid, focus] : references)
    if (!focusUuids.contains(focus)) error(uuid, "Unresolved exported Focus reference: " + focus);
  return result;
}
} // namespace mvr
