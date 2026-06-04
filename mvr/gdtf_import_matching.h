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

#include <algorithm>
#include <filesystem>
#include <string>

namespace mvr {
namespace gdtf_import_matching {

// Trims whitespace from an MVR fixture identity candidate.
inline std::string TrimFixtureIdentity(const std::string &value) {
  const char *whitespace = " \t\r\n";
  const size_t start = value.find_first_not_of(whitespace);
  if (start == std::string::npos)
    return {};
  const size_t end = value.find_last_not_of(whitespace);
  return value.substr(start, end - start + 1);
}

// Normalizes path separators before deriving a fixture identity from GDTFSpec.
inline std::string NormalizeGdtfSpecSeparators(std::string gdtfSpec) {
  std::replace(gdtfSpec.begin(), gdtfSpec.end(), '\\', '/');
  return gdtfSpec;
}

// Extracts the most useful fixture name candidate from an MVR GDTFSpec value.
inline std::string ExtractFixtureNameFromGdtfSpec(const std::string &gdtfSpec) {
  const std::string normalized = TrimFixtureIdentity(NormalizeGdtfSpecSeparators(gdtfSpec));
  if (normalized.empty())
    return {};
  const std::filesystem::path specPath(normalized);
  std::string stem = specPath.filename().stem().string();
  return TrimFixtureIdentity(stem);
}

// Selects the original MVR fixture identity to use for automatic GDTF matching.
inline std::string SelectRequestedFixtureName(const std::string &rawFixtureNodeName,
                                              const std::string &rawGdtfSpec) {
  const std::string fixtureNodeName = TrimFixtureIdentity(rawFixtureNodeName);
  if (!fixtureNodeName.empty())
    return fixtureNodeName;
  return ExtractFixtureNameFromGdtfSpec(rawGdtfSpec);
}

// Selects the fixture name searched in the GDTF catalog, falling back to type.
inline std::string SelectDownloadSearchFixtureName(const std::string &requestedFixtureName,
                                                   const std::string &fixtureTypeName) {
  const std::string requested = TrimFixtureIdentity(requestedFixtureName);
  if (!requested.empty())
    return requested;
  return TrimFixtureIdentity(fixtureTypeName);
}

} // namespace gdtf_import_matching
} // namespace mvr
