/* This file is part of Perastage. */
#pragma once
#include "scene_node_operations.h"
#include "fixture_patch_service.h"

namespace scene_node_operations {
inline perastage::patch::FixturePatchResult ConversionAddresses(const Fixture &fixture) {
  auto result = perastage::patch::ResolveFixturePatch(fixture);
  if (!result || result.addresses || result.primary.empty()) return result;
  auto candidate = fixture;
  candidate.address.clear();
  return perastage::patch::PrepareFixturePrimaryPatch(candidate, result.primary);
}
// Preflight before any hierarchy, selection, related-node or Undo mutation.
inline ConversionResult InspectFixtureToSupport(const MvrScene &scene,
                                                const Fixture &fixture) {
  const auto refuse = [&](const std::string &field, const std::string &reason) {
    ConversionResult result;
    result.uuid = fixture.uuid;
    result.error = "Fixture " + fixture.uuid + "/" + field + ": " + reason;
    result.conflictField = field;
    return result;
  };
  if ((!fixture.gdtfSpec.empty() || !fixture.originalMvrGdtfSpec.empty()) && fixture.gdtfMode.empty())
    return refuse("GDTFMode", "a linked GDTF requires an explicit mode; no replacement is guessed");
  if ((!fixture.gdtfMode.empty() || (fixture.dmxAddresses && !fixture.dmxAddresses->networks.empty())) &&
      fixture.gdtfSpec.empty() && fixture.originalMvrGdtfSpec.empty())
    return refuse("GDTFSpec", "authored modes and Network assignments require a linked GDTF reference");
  for (const auto &issue : scene.preservationIssues)
    if (issue.objectUuid == fixture.uuid)
      return refuse(issue.sourceLocation, "resolve the source preservation conflict before conversion: " + issue.reason);
  for (const auto &[field, present] : {
      std::pair{"Focus", !fixture.focus.empty()},
      std::pair{"Color", !fixture.mvrFixtureColorHex.empty() || !fixture.mvrFixtureColorCie.empty()},
      std::pair{"DMXInvertPan", fixture.dmxInvertPan},
      std::pair{"DMXInvertTilt", fixture.dmxInvertTilt},
      std::pair{"ProjectColor", !fixture.visualColorHex.empty() || fixture.visualColorState != FixtureProjectColorState::Missing},
      std::pair{"ProjectSymbol", !fixture.projectSymbolDefinitionId.empty()},
      std::pair{"PlanningPowerOverride", fixture.planningPowerOverrideW.has_value()},
      std::pair{"PhysicalProperties", fixture.physicalPropertiesDirty ||
          fixture.physicalPropertiesSource == FixturePhysicalPropertiesSource::Manual ||
          fixture.physicalPropertiesSource == FixturePhysicalPropertiesSource::LegacyMvrFixtureNode}})
    if (present) return refuse(field, "Support has no equivalent; retain the Fixture or explicitly clear this field before conversion");
  if (const auto patch = ConversionAddresses(fixture); !patch)
    return refuse("Addresses", patch.detail);
  return {};
}
} // namespace scene_node_operations
