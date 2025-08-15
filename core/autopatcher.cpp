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
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace AutoPatcher {

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

} // namespace AutoPatcher
