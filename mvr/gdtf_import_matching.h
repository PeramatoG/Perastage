/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "gdtf_catalog_matcher.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace mvr {
namespace gdtf_import_matching {

// Identifies an explicit catalog replacement independently of imported labels.
inline std::string BuildSelectedReplacementIdentity(
    const std::string &catalogRevisionId, const std::string &modeName) {
  if (catalogRevisionId.empty())
    return {};
  return std::to_string(catalogRevisionId.size()) + ":" + catalogRevisionId +
         std::to_string(modeName.size()) + ":" + modeName;
}

// Normalizes path separators before deriving a fixture identity from GDTFSpec.
inline std::string NormalizeGdtfSpecSeparators(std::string gdtfSpec) {
  std::replace(gdtfSpec.begin(), gdtfSpec.end(), '\\', '/');
  return gdtfSpec;
}

// Extracts the most useful fixture name candidate from an MVR GDTFSpec value.
inline std::string ExtractFixtureNameFromGdtfSpec(const std::string &gdtfSpec) {
  const std::string normalized =
      gdtf_catalog_matcher::TrimFixtureIdentity(NormalizeGdtfSpecSeparators(gdtfSpec));
  if (normalized.empty())
    return {};
  const std::filesystem::path specPath(normalized);
  std::string stem = specPath.filename().stem().string();
  return gdtf_catalog_matcher::TrimFixtureIdentity(stem);
}

// Selects the original MVR fixture identity to use for automatic GDTF matching.
inline std::string SelectRequestedFixtureName(const std::string &rawFixtureNodeName,
                                              const std::string &rawGdtfSpec) {
  const std::string fixtureNodeName =
      gdtf_catalog_matcher::TrimFixtureIdentity(rawFixtureNodeName);
  if (!fixtureNodeName.empty())
    return fixtureNodeName;
  return ExtractFixtureNameFromGdtfSpec(rawGdtfSpec);
}

// Selects a stable fixture type name when the referenced GDTF file is unavailable.
inline std::string SelectFallbackFixtureTypeName(const std::string &rawFixtureNodeName,
                                                 const std::string &rawGdtfSpec) {
  const std::string specFixtureName = ExtractFixtureNameFromGdtfSpec(rawGdtfSpec);
  if (!specFixtureName.empty())
    return specFixtureName;
  return gdtf_catalog_matcher::TrimFixtureIdentity(rawFixtureNodeName);
}

// Builds catalog evidence with the resolved GDTF identity as authoritative.
inline gdtf_catalog_matcher::GdtfDownloadRequest BuildDownloadRequest(
    const std::string &fixtureNodeAlias, const std::string &resolvedFixtureTypeName,
    const std::string &requestedMode, const std::string &resolvedManufacturer,
    int requestedFootprint) {
  gdtf_catalog_matcher::GdtfDownloadRequest request;
  const std::string authoritative =
      gdtf_catalog_matcher::TrimFixtureIdentity(resolvedFixtureTypeName);
  if (!authoritative.empty())
    request.authoritativeFixtureNames.push_back(authoritative);
  const std::string alias =
      gdtf_catalog_matcher::TrimFixtureIdentity(fixtureNodeAlias);
  if (!alias.empty() && alias != authoritative)
    request.secondaryAliases.push_back(alias);
  request.authoritativeIdentityIsPlaceholder =
      gdtf_catalog_matcher::IsGenericFixtureIdentity(authoritative);
  request.requestedMode = requestedMode;
  request.manufacturer =
      gdtf_catalog_matcher::TrimFixtureIdentity(resolvedManufacturer);
  request.requestedFootprint = requestedFootprint;
  return request;
}

} // namespace gdtf_import_matching
} // namespace mvr
