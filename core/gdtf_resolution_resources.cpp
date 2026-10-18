/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
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
#include "gdtf_resolution_resources.h"
#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"
#include "gdtf/gdtf_mode_channel_browser.h"
#include "uuidutils.h"
#include <algorithm>

namespace gdtf_resolution {
Profile ReadProfile(const std::filesystem::path &path) {
  Profile result;
  const auto archive = gdtf::ReadGdtfArchive(path);
  for (const auto &diagnostic : archive.diagnostics)
    result.diagnostics.push_back(diagnostic.message);
  if (!archive.Success())
    return result;
  std::vector<std::string> paths;
  for (const auto &entry : archive.entries)
    paths.push_back(entry.path);
  const auto description = gdtf::ReadGdtfDescription(archive.descriptionXml, paths);
  for (const auto &diagnostic : description.diagnostics)
    result.diagnostics.push_back(diagnostic.message);
  if (!description.Success())
    return result;
  result.manufacturer = description.manufacturer;
  result.model = description.fixtureTypeName;
  result.fixtureTypeId = description.fixtureTypeId;
  const auto modes = gdtf::ReadGdtfModeChannelDocument(archive.descriptionXml);
  for (const auto &diagnostic : modes.diagnostics) {
    result.diagnostics.push_back(diagnostic.message);
    if (diagnostic.severity == gdtf::GdtfDiagnosticSeverity::Error)
      return result;
  }
  for (const auto &mode : modes.modes)
    if (!mode.name.empty())
      result.modes.push_back({mode.name, mode.calculatedFootprint});
  for (std::size_t i = 0; i < result.modes.size(); ++i)
    for (std::size_t j = i + 1; j < result.modes.size(); ++j)
      if (result.modes[i].name == result.modes[j].name) {
        result.diagnostics.push_back("Duplicate DMX mode names cannot identify a unique mode.");
        return result;
      }
  result.usable = !result.model.empty() && !result.modes.empty();
  if (!result.usable)
    result.diagnostics.push_back("Profile requires a fixture identity and a usable DMX mode.");
  return result;
}

bool AutomaticProfileCompatible(
    const Profile &profile,
    const mvr::gdtf_catalog_matcher::GdtfDownloadRequest &request,
    const std::string &mode) {
  using namespace mvr::gdtf_catalog_matcher;
  if (!HasMode(profile, mode))
    return false;
  if (!IsUnknownManufacturer(request.manufacturer) &&
      NormalizeForGdtfMatch(request.manufacturer) != NormalizeForGdtfMatch(profile.manufacturer))
    return false;
  const auto actual = BuildCanonicalFixtureModel(profile.model, profile.manufacturer);
  bool identityCompatible = false;
  const auto &names = request.authoritativeIdentityIsPlaceholder
      ? request.secondaryAliases : request.authoritativeFixtureNames;
  for (const auto &name : names) {
    const auto expected = BuildCanonicalFixtureModel(name,
        IsUnknownManufacturer(request.manufacturer) ? profile.manufacturer : request.manufacturer);
    identityCompatible |= !actual.canonicalText.empty() &&
        actual.canonicalText == expected.canonicalText;
  }
  const auto selected = std::find_if(profile.modes.begin(), profile.modes.end(),
      [&](const auto &item) { return item.name == mode; });
  return identityCompatible && (request.requestedFootprint <= 0 ||
      selected->footprint == request.requestedFootprint);
}

bool HasMode(const Profile &profile, const std::string &mode) {
  return profile.usable && std::any_of(profile.modes.begin(), profile.modes.end(),
      [&](const auto &item) { return item.name == mode; });
}

std::filesystem::path CopyIntoScene(const std::filesystem::path &source,
                                   const std::filesystem::path &sceneRoot,
                                   std::string &diagnostic) {
  std::error_code error;
  if (sceneRoot.empty() || !std::filesystem::is_directory(sceneRoot, error)) {
    diagnostic = error ? error.message() : "An existing scene resource workspace is required.";
    return {};
  }
  const auto directory = sceneRoot / "resolved-gdtf";
  std::filesystem::create_directories(directory, error);
  if (error) {
    diagnostic = error.message();
    return {};
  }
  const auto destination = directory / (GenerateUuid() + ".gdtf");
  if (!std::filesystem::copy_file(source, destination,
                                 std::filesystem::copy_options::none, error)) {
    diagnostic = error.message();
    std::error_code cleanupError;
    std::filesystem::remove(destination, cleanupError);
    return {};
  }
  return destination;
}
} // namespace gdtf_resolution
