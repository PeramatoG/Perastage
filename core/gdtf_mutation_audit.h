#pragma once

#include <optional>
#include <string>

#include <tinyxml2.h>

namespace GdtfMutationAudit {

// Perastage-controlled schema for mutation metadata persisted in GDTF.
// This version is independent from the GDTF specification version and tracks
// only Perastage mutation-audit semantics.
inline constexpr int kPerastageGdtfMutationSchemaVersion = 1;

enum class CompatibilityMode {
  LegacyFallback,
  KnownPerastageVersion,
  SafeFallbackUnknownVersion,
};

struct CompatibilityDecision {
  CompatibilityMode mode = CompatibilityMode::LegacyFallback;
  std::string warning;
};

// Returns the root <FixtureType> node, creating <GDTF>/<FixtureType> when
// needed for mutation operations.
tinyxml2::XMLElement *EnsureFixtureType(tinyxml2::XMLDocument &doc);

// Decides how to treat Perastage mutation metadata compatibility for a
// FixtureType node.
// - No metadata node: legacy fallback (compatible with older files).
// - Known schema version: treat as trusted Perastage metadata.
// - Unknown schema version: safe fallback with warning.
CompatibilityDecision InspectCompatibility(const tinyxml2::XMLElement *fixtureType);

// Returns the <Revisions> node under fixtureType, creating it when missing.
tinyxml2::XMLElement *EnsureRevisionsNode(tinyxml2::XMLElement *fixtureType,
                                          tinyxml2::XMLDocument &doc);

// Returns a standard "ModifiedBy" value that includes Perastage app version.
std::string BuildPerastageModifiedBy();

// Appends a <Revision> entry under fixtureType.
// - Date: UTC ISO8601 (generated when dateUtcIso8601 is empty)
// - ModifiedBy: provided value, or BuildPerastageModifiedBy() when empty
// - Text: action description
// - UserID: defaults to 0
void AppendRevision(tinyxml2::XMLElement *fixtureType,
                    tinyxml2::XMLDocument &doc,
                    const std::string &text,
                    const std::string &modifiedBy,
                    int userId = 0,
                    const std::string &dateUtcIso8601 = "");

// Stamps Perastage-owned mutation metadata under <FixtureType> and sets
// Editor="Perastage <app version>". Metadata is stored in
// <PerastageMutationAudit>.
void StampPerastageMutationMetadata(tinyxml2::XMLElement *fixtureType,
                                    tinyxml2::XMLDocument &doc);

// Returns the <Properties> node under
// <FixtureType>/<PhysicalDescriptions>/<Properties>, creating missing nodes.
tinyxml2::XMLElement *EnsurePhysicalPropertiesNode(
    tinyxml2::XMLElement *fixtureType, tinyxml2::XMLDocument &doc);

// Applies Weight/PowerConsumption values under PhysicalDescriptions/Properties.
// Only values provided through optionals are written.
// Returns true when at least one property was written.
bool ApplyPhysicalProperties(
    tinyxml2::XMLElement *fixtureType, tinyxml2::XMLDocument &doc,
    const std::optional<float> &weightKg,
    const std::optional<float> &powerConsumptionW);

// Applies physical properties and, when a mutation happened, stamps Perastage
// mutation metadata and appends a revision.
// Returns true when at least one property was written.
bool ApplyPhysicalPropertiesWithAudit(
    tinyxml2::XMLElement *fixtureType, tinyxml2::XMLDocument &doc,
    const std::optional<float> &weightKg,
    const std::optional<float> &powerConsumptionW, const std::string &revisionText,
    const std::string &modifiedBy = "");

} // namespace GdtfMutationAudit
