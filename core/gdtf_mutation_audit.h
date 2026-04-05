#pragma once

#include <string>

#include <tinyxml2.h>

namespace GdtfMutationAudit {

// Perastage-controlled schema for mutation metadata persisted in GDTF.
// This version is independent from the GDTF specification version and tracks
// only Perastage mutation-audit semantics.
inline constexpr int kPerastageGdtfMutationSchemaVersion = 1;

// Returns the root <FixtureType> node, creating <GDTF>/<FixtureType> when
// needed for mutation operations.
tinyxml2::XMLElement *EnsureFixtureType(tinyxml2::XMLDocument &doc);

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
// Editor="Perastage". Metadata is stored in <PerastageMutationAudit>.
void StampPerastageMutationMetadata(tinyxml2::XMLElement *fixtureType,
                                    tinyxml2::XMLDocument &doc);

} // namespace GdtfMutationAudit
