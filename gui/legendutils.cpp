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
#include "legendutils.h"
#include "fixtures/fixture_gdtf_resolution.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
std::string NormalizePath(const std::string &path) {
  std::string out = path;
  char sep = static_cast<char>(fs::path::preferred_separator);
  std::replace(out.begin(), out.end(), '\\', sep);
  std::replace(out.begin(), out.end(), '/', sep);
  return out;
}

std::string NormalizeModelKey(const std::string &path) {
  if (path.empty())
    return {};
  fs::path p(path);
  p = p.lexically_normal();
  return NormalizePath(p.string());
}
} // namespace

std::string BuildFixtureSymbolKey(const Fixture &fixture,
                                  const std::string &basePath) {
  MvrScene scene;
  scene.basePath = basePath;
  gui::fixtures::FixtureGdtfResolution resolution;
  std::string resolutionError;
  gui::fixtures::ResolveFixtureGdtfDeterministic(
      fixture, scene, resolution, resolutionError, "read-layout-legend");
  std::string gdtfPath = resolution.selectedPath;
  std::string modelKey = NormalizeModelKey(gdtfPath);
  if (modelKey.empty() && !fixture.gdtfSpec.empty())
    modelKey = NormalizeModelKey(fixture.gdtfSpec);
  if (modelKey.empty() && !fixture.typeName.empty())
    modelKey = fixture.typeName;
  if (modelKey.empty())
    modelKey = "unknown";
  return modelKey;
}
