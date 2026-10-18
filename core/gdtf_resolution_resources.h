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
#pragma once
#include "../mvr/gdtf_catalog_matcher.h"
#include <filesystem>
#include <string>
#include <vector>

namespace gdtf_resolution {
struct Profile {
  std::string manufacturer;
  std::string model;
  std::string fixtureTypeId;
  std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogModeCandidate> modes;
  std::vector<std::string> diagnostics;
  bool usable = false;
};

// Uses the shared archive, description and mode readers; never mutates the source.
Profile ReadProfile(const std::filesystem::path &path);
bool AutomaticProfileCompatible(
    const Profile &profile,
    const mvr::gdtf_catalog_matcher::GdtfDownloadRequest &request,
    const std::string &mode);
bool HasMode(const Profile &profile, const std::string &mode);
// Destination must be inside an existing import-owned scene workspace.
std::filesystem::path CopyIntoScene(const std::filesystem::path &source,
                                   const std::filesystem::path &sceneRoot,
                                   std::string &diagnostic);
} // namespace gdtf_resolution
