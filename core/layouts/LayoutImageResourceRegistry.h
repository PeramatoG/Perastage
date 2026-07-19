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

#include "LayoutCollection.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace layouts {

class LayoutImageResourceRegistry {
public:
  struct ResourceEntry {
    std::string archivePath;
    std::string originalPath;
    std::vector<std::uint8_t> bytes;
    int useCount = 0;
  };

  static LayoutImageResourceRegistry &Get();

  bool AttachResource(LayoutImageDefinition &image);
  void SynchronizeWithLayouts(const LayoutCollection &layouts);
  void Clear();

  std::vector<ResourceEntry> UsedResources() const;
  int UsageCount(const std::string &archivePath) const;
  bool HasResourceBytes(const std::string &archivePath) const;
  bool GetResourceBytes(const std::string &archivePath,
                        std::vector<std::uint8_t> &out) const;
  void RegisterResourceBytes(const std::string &archivePath,
                             const std::string &originalPath,
                             const std::vector<std::uint8_t> &bytes);

private:
  LayoutImageResourceRegistry() = default;

  std::unordered_map<std::string, ResourceEntry> resources;
};

} // namespace layouts
