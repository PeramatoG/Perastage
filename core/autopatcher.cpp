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
#include "autopatcher.h"
#include "gdtfloader.h"
#include "patchmanager.h"
#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace AutoPatcher {
namespace {

std::optional<PatchManager::PatchAddress>
ParsePatchAddress(const std::string &address) {
  const size_t dotPos = address.find('.');
  if (dotPos == std::string::npos || dotPos == 0 || dotPos + 1 >= address.size())
    return std::nullopt;

  try {
    const int universe = std::stoi(address.substr(0, dotPos));
    const int channel = std::stoi(address.substr(dotPos + 1));
    if (universe < 1 || channel < 1 || channel > 512)
      return std::nullopt;
    return PatchManager::PatchAddress{universe, channel};
  } catch (...) {
    return std::nullopt;
  }
}

int ResolveFixtureChannelCount(const MvrScene &scene, const Fixture &fixture) {
  std::string fullPath;
  if (!fixture.gdtfSpec.empty()) {
    fs::path p = scene.basePath.empty()
                     ? fs::path(fixture.gdtfSpec)
                     : fs::path(scene.basePath) / fixture.gdtfSpec;
    fullPath = p.string();
  }
  int channelCount = GetGdtfModeChannelCount(fullPath, fixture.gdtfMode);
  return channelCount > 0 ? channelCount : 1;
}

std::optional<PatchManager::PatchAddress>
FindNextAddressAfterHighestPatchedFixture(const MvrScene &scene,
                                          const std::unordered_set<std::string> &ignoredUuids) {
  int highestAbsoluteEnd = 0;

  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (ignoredUuids.contains(uuid))
      continue;
    const std::optional<PatchManager::PatchAddress> startAddress =
        ParsePatchAddress(fixture.address);
    if (!startAddress)
      continue;

    const int channelCount = ResolveFixtureChannelCount(scene, fixture);
    const int absoluteStart = (startAddress->universe - 1) * 512 + startAddress->channel;
    const int absoluteEnd = absoluteStart + channelCount - 1;
    highestAbsoluteEnd = std::max(highestAbsoluteEnd, absoluteEnd);
  }

  if (highestAbsoluteEnd <= 0)
    return PatchManager::PatchAddress{1, 1};

  const int nextAbsolute = highestAbsoluteEnd + 1;
  const int universe = ((nextAbsolute - 1) / 512) + 1;
  const int channel = ((nextAbsolute - 1) % 512) + 1;
  return PatchManager::PatchAddress{universe, channel};
}
} // namespace

void AutoPatch(MvrScene &scene, int startUniverse, int startChannel) {
  struct FixtureInfo {
    std::string uuid;
    int channels;
    float x;
    float y;
    std::string type;
    std::string hang;
  };

  std::vector<FixtureInfo> fixtures;
  fixtures.reserve(scene.fixtures.size());

  for (auto &pair : scene.fixtures) {
    auto &f = pair.second;
    std::string fullPath;
    if (!f.gdtfSpec.empty()) {
      fs::path p = scene.basePath.empty()
                       ? fs::path(f.gdtfSpec)
                       : fs::path(scene.basePath) / f.gdtfSpec;
      fullPath = p.string();
    }
    int chCount = GetGdtfModeChannelCount(fullPath, f.gdtfMode);
    if (chCount <= 0)
      continue; // skip fixtures without a valid channel count
    auto pos = f.GetPosition();
    fixtures.push_back(
        {pair.first, chCount, pos[0], pos[1], f.typeName, f.positionName});
  }

  std::sort(fixtures.begin(), fixtures.end(),
            [](const FixtureInfo &a, const FixtureInfo &b) {
              if (a.y == b.y) {
                if (a.hang == b.hang) {
                  if (a.type == b.type)
                    return a.x < b.x;
                  return a.type < b.type;
                }
                return a.hang < b.hang;
              }
              return a.y < b.y;
            });

  struct Group {
    std::vector<size_t> indices;
    int total = 0;
  };

  std::vector<Group> groups;
  // Reserve upfront to avoid repeated reallocations when processing large
  // rigs where fixtures.size() can be in the hundreds.
  groups.reserve(fixtures.size());
  for (size_t i = 0; i < fixtures.size(); ++i) {
    const auto &f = fixtures[i];
    if (groups.empty()) {
      groups.push_back({{i}, f.channels});
      continue;
    }

    const auto &last = fixtures[groups.back().indices.front()];
    if (last.hang == f.hang && last.type == f.type) {
      groups.back().indices.push_back(i);
      groups.back().total += f.channels;
    } else {
      groups.push_back({{i}, f.channels});
    }
  }

  int uni = startUniverse < 1 ? 1 : startUniverse;
  int ch = startChannel < 1 ? 1 : startChannel;

  for (const auto &g : groups) {
    if (g.total <= 512 && ch + g.total - 1 > 512) {
      ++uni;
      ch = 1;
    }

    for (size_t idx : g.indices) {
      const auto &f = fixtures[idx];
      if (ch + f.channels - 1 > 512) {
        ++uni;
        ch = 1;
      }

      scene.fixtures[f.uuid].address =
          std::to_string(uni) + "." + std::to_string(ch);

      ch += f.channels;
      if (ch > 512) {
        ++uni;
        ch = 1;
      }
    }
  }
}

void AutoPatchSelection(MvrScene &scene,
                        const std::vector<std::string> &selectionOrder) {
  if (selectionOrder.empty())
    return;

  std::unordered_set<std::string> uniqueSelection(selectionOrder.begin(),
                                                  selectionOrder.end());

  for (const auto &uuid : uniqueSelection) {
    auto fixtureIt = scene.fixtures.find(uuid);
    if (fixtureIt != scene.fixtures.end())
      fixtureIt->second.address.clear();
  }

  const auto nextStart =
      FindNextAddressAfterHighestPatchedFixture(scene, uniqueSelection);
  if (!nextStart)
    return;

  std::vector<int> channelCounts;
  std::vector<std::string> orderedFixtureUuids;
  channelCounts.reserve(selectionOrder.size());
  orderedFixtureUuids.reserve(selectionOrder.size());

  std::unordered_set<std::string> seen;
  for (const auto &uuid : selectionOrder) {
    if (!seen.insert(uuid).second)
      continue;

    const auto fixtureIt = scene.fixtures.find(uuid);
    if (fixtureIt == scene.fixtures.end())
      continue;

    orderedFixtureUuids.push_back(uuid);
    channelCounts.push_back(ResolveFixtureChannelCount(scene, fixtureIt->second));
  }

  if (orderedFixtureUuids.empty())
    return;

  const std::vector<PatchManager::PatchAddress> addresses =
      PatchManager::SequentialPatch(channelCounts, nextStart->universe,
                                    nextStart->channel);

  for (size_t i = 0; i < orderedFixtureUuids.size() && i < addresses.size(); ++i) {
    const auto &patch = addresses[i];
    scene.fixtures[orderedFixtureUuids[i]].address =
        std::to_string(patch.universe) + "." + std::to_string(patch.channel);
  }
}

} // namespace AutoPatcher
