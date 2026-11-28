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

#include <string>
#include <vector>

struct LayoutViewPreset {
  std::string name;
  std::vector<std::string> showPanes;
  std::vector<std::string> hidePanes;
};

class LayoutViewPresetRegistry {
public:
  static const LayoutViewPreset *GetPreset(const std::string &name);
  static const std::vector<LayoutViewPreset> &GetPresets();
};

// Preserve older project perspectives while keeping the new pane user-owned.
inline std::string WithoutLayoutPropertiesPane(std::string perspective) {
  constexpr auto pane = "|name=LayoutProperties;";
  const auto start = perspective.find(pane);
  if (start == std::string::npos) return perspective;
  auto end = start + 1;
  for (; end < perspective.size(); ++end) {
    if (perspective[end] == '\\' && end + 1 < perspective.size()) ++end;
    else if (perspective[end] == '|') break;
  }
  perspective.erase(start, end - start);
  return perspective;
}
