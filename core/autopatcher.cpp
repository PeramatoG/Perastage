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
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace AutoPatcher {
namespace {

constexpr double kCoordinateNoiseFloor = 0.01;
constexpr double kComponentGapFactor = 3.0;

enum class Orientation { Transverse, Longitudinal };
enum class PatchTraversalMode { Serpentine, SameDirection };

struct FixturePatchInfo {
  std::string uuid;
  int channels = 0;
  double x = 0.0;
  double y = 0.0;
  std::string type;
};

struct FixtureTypeGroup {
  std::string type;
  std::vector<FixturePatchInfo *> fixtures;
  int channels = 0;
};

struct PhysicalPatchComponent {
  std::vector<FixturePatchInfo *> fixtures;
  std::vector<FixtureTypeGroup> typeGroups;
  Orientation orientation = Orientation::Transverse;
  bool reverseTraversal = false;
  double centerX = 0.0;
  double centerY = 0.0;
  int channels = 0;
};

struct LogicalPatchPosition {
  std::string key;
  std::vector<FixturePatchInfo *> fixtures;
  std::vector<PhysicalPatchComponent> components;
  Orientation orientation = Orientation::Transverse;
  double centerX = 0.0;
  double centerY = 0.0;
  int channels = 0;
};

// Parses a stored universe.channel patch address.
std::optional<PatchManager::PatchAddress>
ParsePatchAddress(const std::string &address) {
  const size_t dotPos = address.find('.');
  if (dotPos == std::string::npos || dotPos == 0 ||
      dotPos + 1 >= address.size())
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

// Resolves a fixture's footprint from its GDTF mode with a safe fallback.
int ResolveFixtureChannelCount(const MvrScene &scene, const Fixture &fixture) {
  std::string fullPath;
  if (!fixture.gdtfSpec.empty()) {
    const fs::path path = scene.basePath.empty()
                              ? fs::path(fixture.gdtfSpec)
                              : fs::path(scene.basePath) / fixture.gdtfSpec;
    fullPath = path.string();
  }
  const int channelCount = GetGdtfModeChannelCount(fullPath, fixture.gdtfMode);
  return channelCount > 0 ? channelCount : 1;
}

// Normalizes a display name for deterministic fallback position identity.
std::string NormalizePositionName(const std::string &name) {
  std::string normalized;
  bool pendingSpace = false;
  for (const unsigned char value : name) {
    if (std::isspace(value)) {
      pendingSpace = !normalized.empty();
      continue;
    }
    if (pendingSpace) {
      normalized.push_back(' ');
      pendingSpace = false;
    }
    normalized.push_back(static_cast<char>(std::toupper(value)));
  }
  return normalized;
}

// Selects the standards-based logical Position identity with safe fallbacks.
std::string LogicalPositionKey(const Fixture &fixture) {
  if (!fixture.position.empty())
    return "uuid:" + fixture.position;
  const std::string normalizedName =
      NormalizePositionName(fixture.positionName);
  if (!normalizedName.empty())
    return "name:" + normalizedName;
  // A noise-tolerant Y row preserves the old front-to-back behavior without
  // pretending that every metadata-free fixture shares one semantic Position.
  const auto coordinates = fixture.GetPosition();
  const long long row =
      std::llround(static_cast<double>(coordinates[1]) / kCoordinateNoiseFloor);
  return "legacy-row:" + std::to_string(row);
}

// Calculates planar distance between two patch fixtures.
double Distance(const FixturePatchInfo &a, const FixturePatchInfo &b) {
  return std::hypot(a.x - b.x, a.y - b.y);
}

// Detects disconnected structures by cutting anomalously long MST edges.
std::vector<std::vector<FixturePatchInfo *>>
ClusterPhysicalComponents(const std::vector<FixturePatchInfo *> &fixtures) {
  if (fixtures.size() < 3)
    return {fixtures};

  struct Edge {
    size_t a = 0;
    size_t b = 0;
    double distance = 0.0;
  };

  std::vector<double> nearest(fixtures.size(),
                              std::numeric_limits<double>::max());
  for (size_t i = 0; i < fixtures.size(); ++i) {
    for (size_t j = i + 1; j < fixtures.size(); ++j) {
      const double distance = Distance(*fixtures[i], *fixtures[j]);
      if (distance > kCoordinateNoiseFloor) {
        nearest[i] = std::min(nearest[i], distance);
        nearest[j] = std::min(nearest[j], distance);
      }
    }
  }
  nearest.erase(std::remove_if(nearest.begin(), nearest.end(),
                               [](double value) {
                                 return !std::isfinite(value) ||
                                        value ==
                                            std::numeric_limits<double>::max();
                               }),
                nearest.end());
  if (nearest.empty())
    return {fixtures};
  std::sort(nearest.begin(), nearest.end());
  const double localSpacing = nearest[nearest.size() / 2];
  const double cutThreshold =
      std::max(kCoordinateNoiseFloor, localSpacing * kComponentGapFactor);

  std::vector<bool> included(fixtures.size(), false);
  std::vector<Edge> mst;
  included[0] = true;
  for (size_t count = 1; count < fixtures.size(); ++count) {
    Edge best{0, 0, std::numeric_limits<double>::max()};
    for (size_t i = 0; i < fixtures.size(); ++i) {
      if (!included[i])
        continue;
      for (size_t j = 0; j < fixtures.size(); ++j) {
        if (included[j])
          continue;
        const double distance = Distance(*fixtures[i], *fixtures[j]);
        if (distance < best.distance ||
            (std::abs(distance - best.distance) <= kCoordinateNoiseFloor &&
             fixtures[j]->uuid < fixtures[best.b]->uuid))
          best = {i, j, distance};
      }
    }
    included[best.b] = true;
    mst.push_back(best);
  }

  std::vector<size_t> parent(fixtures.size());
  std::iota(parent.begin(), parent.end(), 0);
  const auto findRoot = [&parent](size_t value) {
    size_t root = value;
    while (parent[root] != root)
      root = parent[root];
    while (parent[value] != value) {
      const size_t next = parent[value];
      parent[value] = root;
      value = next;
    }
    return root;
  };
  for (const Edge &edge : mst) {
    if (edge.distance <= cutThreshold) {
      const size_t a = findRoot(edge.a);
      const size_t b = findRoot(edge.b);
      parent[b] = a;
    }
  }

  std::map<size_t, std::vector<FixturePatchInfo *>> clusters;
  for (size_t i = 0; i < fixtures.size(); ++i)
    clusters[findRoot(i)].push_back(fixtures[i]);
  std::vector<std::vector<FixturePatchInfo *>> result;
  for (auto &[root, cluster] : clusters) {
    (void)root;
    std::sort(cluster.begin(), cluster.end(),
              [](const auto *a, const auto *b) { return a->uuid < b->uuid; });
    result.push_back(std::move(cluster));
  }
  return result;
}

// Classifies a component by its dominant planar range.
Orientation
ClassifyOrientation(const std::vector<FixturePatchInfo *> &fixtures) {
  double minX = fixtures.front()->x;
  double maxX = minX;
  double minY = fixtures.front()->y;
  double maxY = minY;
  for (const FixturePatchInfo *fixture : fixtures) {
    minX = std::min(minX, fixture->x);
    maxX = std::max(maxX, fixture->x);
    minY = std::min(minY, fixture->y);
    maxY = std::max(maxY, fixture->y);
  }
  return maxY - minY > maxX - minX + kCoordinateNoiseFloor
             ? Orientation::Longitudinal
             : Orientation::Transverse;
}

// Builds stable type groups and orders each group along the component axis.
void BuildFixtureTypeGroups(PhysicalPatchComponent &component) {
  std::map<std::string, std::vector<FixturePatchInfo *>> byType;
  for (FixturePatchInfo *fixture : component.fixtures)
    byType[fixture->type].push_back(fixture);

  for (auto &[type, fixtures] : byType) {
    std::sort(
        fixtures.begin(), fixtures.end(), [&](const auto *a, const auto *b) {
          const double primaryA =
              component.orientation == Orientation::Transverse ? a->x : a->y;
          const double primaryB =
              component.orientation == Orientation::Transverse ? b->x : b->y;
          if (std::abs(primaryA - primaryB) > kCoordinateNoiseFloor)
            return component.reverseTraversal ? primaryA > primaryB
                                              : primaryA < primaryB;
          const double secondaryA =
              component.orientation == Orientation::Transverse ? a->y : a->x;
          const double secondaryB =
              component.orientation == Orientation::Transverse ? b->y : b->x;
          if (std::abs(secondaryA - secondaryB) > kCoordinateNoiseFloor)
            return secondaryA < secondaryB;
          return a->uuid < b->uuid;
        });
    FixtureTypeGroup group{type, std::move(fixtures), 0};
    for (const FixturePatchInfo *fixture : group.fixtures)
      group.channels += fixture->channels;
    component.typeGroups.push_back(std::move(group));
  }
}

// Builds topology and applies the isolated component traversal policy.
void BuildPhysicalComponents(LogicalPatchPosition &position,
                             PatchTraversalMode traversalMode) {
  for (auto &cluster : ClusterPhysicalComponents(position.fixtures)) {
    PhysicalPatchComponent component;
    component.fixtures = std::move(cluster);
    component.orientation = ClassifyOrientation(component.fixtures);
    for (const FixturePatchInfo *fixture : component.fixtures) {
      component.centerX += fixture->x;
      component.centerY += fixture->y;
      component.channels += fixture->channels;
    }
    component.centerX /= component.fixtures.size();
    component.centerY /= component.fixtures.size();
    position.components.push_back(std::move(component));
  }

  const size_t longitudinalCount = std::count_if(
      position.components.begin(), position.components.end(),
      [](const auto &c) { return c.orientation == Orientation::Longitudinal; });
  position.orientation = longitudinalCount * 2 >= position.components.size()
                             ? Orientation::Longitudinal
                             : Orientation::Transverse;
  std::sort(position.components.begin(), position.components.end(),
            [&](const auto &a, const auto &b) {
              const double primaryA =
                  position.orientation == Orientation::Longitudinal ? a.centerX
                                                                    : a.centerY;
              const double primaryB =
                  position.orientation == Orientation::Longitudinal ? b.centerX
                                                                    : b.centerY;
              if (std::abs(primaryA - primaryB) > kCoordinateNoiseFloor)
                return primaryA < primaryB;
              if (std::abs(a.centerX - b.centerX) > kCoordinateNoiseFloor)
                return a.centerX < b.centerX;
              return a.fixtures.front()->uuid < b.fixtures.front()->uuid;
            });
  for (size_t i = 0; i < position.components.size(); ++i) {
    position.components[i].reverseTraversal =
        traversalMode == PatchTraversalMode::Serpentine && i % 2 == 1;
    BuildFixtureTypeGroups(position.components[i]);
  }
}

// Collects fixtures and groups them by standard Position identity.
std::vector<LogicalPatchPosition>
BuildLogicalPatchPositions(MvrScene &scene,
                           std::vector<FixturePatchInfo> &fixtureStorage) {
  std::vector<std::string> uuids;
  uuids.reserve(scene.fixtures.size());
  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)fixture;
    uuids.push_back(uuid);
  }
  std::sort(uuids.begin(), uuids.end());

  std::map<std::string, std::vector<FixturePatchInfo *>> grouped;
  for (const std::string &uuid : uuids) {
    const Fixture &fixture = scene.fixtures.at(uuid);
    const int channels = ResolveFixtureChannelCount(scene, fixture);
    const auto coordinates = fixture.GetPosition();
    fixtureStorage.push_back(
        {uuid, channels, coordinates[0], coordinates[1], fixture.typeName});
    grouped[LogicalPositionKey(fixture)].push_back(&fixtureStorage.back());
  }

  std::vector<LogicalPatchPosition> positions;
  for (auto &[key, fixtures] : grouped) {
    LogicalPatchPosition position;
    position.key = key;
    position.fixtures = std::move(fixtures);
    for (const FixturePatchInfo *fixture : position.fixtures) {
      position.centerX += fixture->x;
      position.centerY += fixture->y;
      position.channels += fixture->channels;
    }
    position.centerX /= position.fixtures.size();
    position.centerY /= position.fixtures.size();
    BuildPhysicalComponents(position, PatchTraversalMode::Serpentine);
    positions.push_back(std::move(position));
  }

  std::sort(positions.begin(), positions.end(),
            [](const auto &a, const auto &b) {
              if (a.orientation != b.orientation)
                return a.orientation == Orientation::Transverse;
              if (std::abs(a.centerY - b.centerY) > kCoordinateNoiseFloor)
                return a.centerY < b.centerY;
              if (std::abs(a.centerX - b.centerX) > kCoordinateNoiseFloor)
                return a.centerX < b.centerX;
              return a.key < b.key;
            });
  return positions;
}

// Advances to a fresh universe when a protected block cannot fit completely.
void ProtectBlock(int blockChannels, int &universe, int &channel) {
  if (blockChannels <= 512 && channel + blockChannels - 1 > 512) {
    ++universe;
    channel = 1;
  }
}

// Assigns one fixture without allowing it to cross a universe boundary.
void AssignFixture(MvrScene &scene, const FixturePatchInfo &fixture,
                   int &universe, int &channel) {
  if (channel + fixture.channels - 1 > 512) {
    ++universe;
    channel = 1;
  }
  scene.fixtures.at(fixture.uuid).address =
      std::to_string(universe) + "." + std::to_string(channel);
  channel += fixture.channels;
  if (channel > 512) {
    ++universe;
    channel = 1;
  }
}

// Packs logical positions with position, component, and type protection levels.
void PackLogicalPositions(MvrScene &scene,
                          const std::vector<LogicalPatchPosition> &positions,
                          int startUniverse, int startChannel) {
  int universe = std::max(1, startUniverse);
  int channel = std::clamp(startChannel, 1, 512);
  for (const LogicalPatchPosition &position : positions) {
    ProtectBlock(position.channels, universe, channel);
    for (const PhysicalPatchComponent &component : position.components) {
      if (position.channels > 512)
        ProtectBlock(component.channels, universe, channel);
      for (const FixtureTypeGroup &group : component.typeGroups) {
        if (component.channels > 512)
          ProtectBlock(group.channels, universe, channel);
        for (const FixturePatchInfo *fixture : group.fixtures)
          AssignFixture(scene, *fixture, universe, channel);
      }
    }
  }
}

// Finds the first address after every fixture not included in a selection.
std::optional<PatchManager::PatchAddress>
FindNextAddressAfterHighestPatchedFixture(
    const MvrScene &scene,
    const std::unordered_set<std::string> &ignoredUuids) {
  int highestAbsoluteEnd = 0;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (ignoredUuids.contains(uuid))
      continue;
    const auto startAddress = ParsePatchAddress(fixture.address);
    if (!startAddress)
      continue;
    const int absoluteStart =
        (startAddress->universe - 1) * 512 + startAddress->channel;
    highestAbsoluteEnd = std::max(
        highestAbsoluteEnd,
        absoluteStart + ResolveFixtureChannelCount(scene, fixture) - 1);
  }
  if (highestAbsoluteEnd <= 0)
    return PatchManager::PatchAddress{1, 1};
  const int nextAbsolute = highestAbsoluteEnd + 1;
  return PatchManager::PatchAddress{((nextAbsolute - 1) / 512) + 1,
                                    ((nextAbsolute - 1) % 512) + 1};
}

} // namespace

// Automatically patches fixtures using logical and physical rig topology.
void AutoPatch(MvrScene &scene, int startUniverse, int startChannel) {
  std::vector<FixturePatchInfo> fixtureStorage;
  fixtureStorage.reserve(scene.fixtures.size());
  const std::vector<LogicalPatchPosition> positions =
      BuildLogicalPatchPositions(scene, fixtureStorage);
  PackLogicalPositions(scene, positions, startUniverse, startChannel);
}

// Re-patches selected fixtures in their exact user-provided order.
void AutoPatchSelection(MvrScene &scene,
                        const std::vector<std::string> &selectionOrder) {
  if (selectionOrder.empty())
    return;
  std::unordered_set<std::string> uniqueSelection(selectionOrder.begin(),
                                                  selectionOrder.end());
  for (const std::string &uuid : uniqueSelection) {
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
  std::unordered_set<std::string> seen;
  for (const std::string &uuid : selectionOrder) {
    if (!seen.insert(uuid).second)
      continue;
    const auto fixtureIt = scene.fixtures.find(uuid);
    if (fixtureIt == scene.fixtures.end())
      continue;
    orderedFixtureUuids.push_back(uuid);
    channelCounts.push_back(
        ResolveFixtureChannelCount(scene, fixtureIt->second));
  }
  const std::vector<PatchManager::PatchAddress> addresses =
      PatchManager::SequentialPatch(channelCounts, nextStart->universe,
                                    nextStart->channel);
  for (size_t i = 0; i < orderedFixtureUuids.size() && i < addresses.size();
       ++i) {
    scene.fixtures[orderedFixtureUuids[i]].address =
        std::to_string(addresses[i].universe) + "." +
        std::to_string(addresses[i].channel);
  }
}

} // namespace AutoPatcher
