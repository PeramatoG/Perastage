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
#include "../../viewer2d/fixture_label_style.h"
#include <map>
#include <stdexcept>

namespace layouts {
// A field is the atomic inheritance unit. Missing fields always inherit.
struct FixtureLabelStyleProfile {
  std::string id, name;
  viewer2d::LabelFieldStyles fields;
  bool operator==(const FixtureLabelStyleProfile &) const = default;
};
using FixtureLabelProfiles = std::map<std::string, FixtureLabelStyleProfile>;
struct FixtureLabelPresentation {
  std::string profileId;
  viewer2d::LabelFieldStyles fields;
  std::map<std::string, viewer2d::LabelFieldStyles> fixtures;
  bool operator==(const FixtureLabelPresentation &) const = default;
};
} // namespace layouts
