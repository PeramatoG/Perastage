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
#include "layoutlegenditems.h"

#include <filesystem>
#include <map>

#include <wx/filename.h>

#include "configmanager.h"
#include "gdtfloader.h"
#include "guiconfigservices.h"
#include "legendutils.h"

namespace {
struct LegendAggregate {
  int count = 0;
  std::optional<int> channelCount;
  bool mixedChannels = false;
  std::string symbolKey;
};
} // namespace

std::vector<SharedLayoutLegendItem> BuildSharedLayoutLegendItems() {
  std::map<std::string, LegendAggregate> aggregates;
  const auto &fixtures =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene().fixtures;
  const std::string &basePath =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene().basePath;

  for (const auto &[uuid, fixture] : fixtures) {
    (void)uuid;
    std::string typeName = fixture.typeName;
    std::string fullPath;
    if (!fixture.gdtfSpec.empty()) {
      try {
        std::filesystem::path p =
            basePath.empty() ? std::filesystem::u8path(fixture.gdtfSpec)
                             : std::filesystem::u8path(basePath) /
                                   std::filesystem::u8path(fixture.gdtfSpec);
        fullPath = p.string();
      } catch (const std::exception &) {
        fullPath.clear();
      }
    }

    if (typeName.empty() && !fullPath.empty()) {
      wxFileName fn(fullPath);
      typeName = fn.GetFullName().ToStdString();
    }
    if (typeName.empty())
      typeName = "Unknown";

    int chCount = GetGdtfModeChannelCount(fullPath, fixture.gdtfMode);
    const std::string symbolKey = BuildFixtureSymbolKey(fixture, basePath);

    LegendAggregate &agg = aggregates[typeName];
    agg.count += 1;
    if (chCount >= 0) {
      if (!agg.channelCount.has_value()) {
        agg.channelCount = chCount;
      } else if (agg.channelCount.value() != chCount) {
        agg.mixedChannels = true;
      }
    }

    // Keep the first available symbol key as the legend representative for
    // the fixture type. This ensures every row can render a generated vector
    // symbol even when the type contains mixed fixture variants.
    if (agg.symbolKey.empty() && !symbolKey.empty())
      agg.symbolKey = symbolKey;
  }

  std::vector<SharedLayoutLegendItem> items;
  items.reserve(aggregates.size());
  for (const auto &[typeName, agg] : aggregates) {
    SharedLayoutLegendItem item;
    item.typeName = typeName;
    item.count = agg.count;
    if (agg.channelCount.has_value() && !agg.mixedChannels)
      item.channelCount = agg.channelCount;
    item.symbolKey = agg.symbolKey;
    items.push_back(item);
  }

  if (items.empty()) {
    SharedLayoutLegendItem item;
    item.typeName = "No fixtures";
    item.count = 0;
    items.push_back(item);
  }

  return items;
}

