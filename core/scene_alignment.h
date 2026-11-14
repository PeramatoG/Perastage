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
#include "alignment_geometry.h"
#include "command/command_execution.h"
#include <functional>
#include <optional>

namespace alignment {
using Target = scene_grouping::SceneTransformTarget;
using BoundsProvider = std::function<std::optional<Bounds>(const MvrScene &, const Target &, std::string &)>;
struct Targets {
  std::vector<Target> items;
  std::string diagnostic;
};
// Normalizes with the shared interactive policy, retaining first occurrence order.
Targets ResolveTargets(const MvrScene &scene, const std::vector<Target> &ordered,
                       const scene_grouping::InteractiveTransformPolicy &policy);
// Captures geometry, validates the whole plan, then delegates atomic publication.
perastage::command::Result Execute(
    perastage::command::ExecutionContext &context, const std::vector<Target> &ordered,
    Reference reference, Operation operation, Plane plane,
    const scene_grouping::InteractiveTransformPolicy &policy,
    const BoundsProvider &bounds);
} // namespace alignment
