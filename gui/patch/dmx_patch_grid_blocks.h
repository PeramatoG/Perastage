/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include "patch/dmx_patch_snapshot.h"
#include <map>
#include <set>
namespace dmx_patch_ui {
inline std::set<std::string> FixtureIds(const dmx_patch::Cell &cell) {
  std::set<std::string> ids;
  for (const auto &entry : cell.occupants) ids.insert(entry.uuid);
  for (const auto &entry : cell.provisional) ids.insert(entry.uuid);
  return ids;
}
// Bounds span row wraps and sparse holes. A fixture is named only at its first
// displayed byte, independent of painting order or the current scroll position.
struct GridBlocks {
  std::map<std::string, std::pair<int, int>> bounds;
  explicit GridBlocks(const dmx_patch::Snapshot &snapshot) {
    for (int i = 0; i < 512; ++i)
      for (const auto &uuid : FixtureIds(snapshot.cells[i])) {
        auto [it, inserted] = bounds.try_emplace(uuid, i, i);
        it->second.second = i;
      }
  }
  bool Starts(int index) const {
    for (const auto &[uuid, range] : bounds) if (range.first == index) return true;
    return false;
  }
  bool Ends(int index) const {
    for (const auto &[uuid, range] : bounds) if (range.second == index) return true;
    return false;
  }
  std::vector<std::string> NamesAt(int index) const {
    std::vector<std::string> names;
    for (const auto &[uuid, range] : bounds) if (range.first == index) names.push_back(uuid);
    return names;
  }
};
}
