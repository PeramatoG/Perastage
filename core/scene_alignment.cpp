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
#include "scene_alignment.h"
#include "command/command_transform_batch.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace alignment {
namespace {
scene_grouping::ObjectSelection Selection(const std::vector<Target> &targets) {
  scene_grouping::ObjectSelection selection;
  for (const auto &target : targets) {
    switch (target.type) {
    case MvrNodeType::Fixture: selection.fixtures.push_back(target.uuid); break;
    case MvrNodeType::Truss: selection.trusses.push_back(target.uuid); break;
    case MvrNodeType::Support: selection.supports.push_back(target.uuid); break;
    case MvrNodeType::SceneObject: selection.sceneObjects.push_back(target.uuid); break;
    case MvrNodeType::GroupObject: selection.groupObjects.push_back(target.uuid); break;
    }
  }
  return selection;
}
bool Same(const Target &a, const Target &b) { return a.type == b.type && a.uuid == b.uuid; }
const char *Kind(MvrNodeType type) {
  switch (type) {
  case MvrNodeType::Fixture: return "fixture";
  case MvrNodeType::Truss: return "truss";
  case MvrNodeType::Support: return "support";
  case MvrNodeType::SceneObject: return "scene_object";
  case MvrNodeType::GroupObject: return "group";
  }
  return "";
}
}
Targets ResolveTargets(const MvrScene &scene, const std::vector<Target> &ordered,
                       const scene_grouping::InteractiveTransformPolicy &policy) {
  Targets result;
  const auto effective = scene_grouping::BuildInteractiveTransformTargets(scene, Selection(ordered), policy);
  for (const auto &identity : ordered) {
    const auto exact = scene_grouping::BuildExactTransformTargets(scene, Selection({identity}));
    if (exact.size() != 1) {
      result.diagnostic = "Selected object is missing: " + identity.uuid;
      result.items.clear(); return result;
    }
    // Full-set suppression may remove a leaf selected alongside its promoted root.
    // Resolve that identity through the same policy with promotion enabled for it.
    auto single = scene_grouping::BuildInteractiveTransformTargets(scene, Selection({identity}), policy);
    auto match = std::find_if(effective.begin(), effective.end(), [&](const Target &t) {
      return !single.empty() && Same(t, single.front());
    });
    if (match == effective.end()) {
      auto promote = policy;
      promote.promoteFixturesToGroup = promote.promoteTrussesToGroup = true;
      promote.promoteSupportsToGroup = promote.promoteSceneObjectsToGroup = true;
      single = scene_grouping::BuildInteractiveTransformTargets(scene, Selection({identity}), promote);
      match = std::find_if(effective.begin(), effective.end(), [&](const Target &t) {
        return !single.empty() && Same(t, single.front());
      });
    }
    if (match == effective.end()) {
      result.diagnostic = "Selection promotion is ambiguous: " + identity.uuid;
      result.items.clear(); return result;
    }
    if (std::none_of(result.items.begin(), result.items.end(), [&](const Target &t) { return Same(t,*match); }))
      result.items.push_back(*match);
  }
  return result;
}
perastage::command::Result Execute(
    perastage::command::ExecutionContext &context, const std::vector<Target> &ordered,
    Reference reference, Operation operation, Plane plane,
    const scene_grouping::InteractiveTransformPolicy &policy, const BoundsProvider &provider) {
  using namespace perastage::command;
  Result result;
  result.request = Request{"scene.align", {}};
  auto reject = [&](const std::string &message) {
    result.outcome = Outcome::ValidationError;
    result.diagnostics.push_back({DiagnosticSeverity::Error, DiagnosticPhase::Validation,
                                 "scene.align.invalid_geometry", message});
    return result;
  };
  try {
    const auto targets = ResolveTargets(context.scene, ordered, policy);
    if (!targets.diagnostic.empty()) return reject(targets.diagnostic);
    if (targets.items.size() < 2) return reject("Select at least two distinct effective transform targets.");
    std::vector<Rectangle> rectangles;
    for (const auto &target : targets.items) {
      std::string diagnostic;
      const auto bounds = provider(context.scene, target, diagnostic);
      if (!bounds || !Valid(*bounds))
        return reject("Physical bounds unavailable for " + target.uuid + ": " + diagnostic);
      rectangles.push_back(Project(*bounds, plane));
    }
    const auto plan = Calculate(rectangles, reference, operation);
    if (!plan.valid) return reject(plan.diagnostic);
    if (!plan.changed) return result;
    std::vector<std::string> kinds, uuids, components, axes, modes, spaces;
    std::vector<double> values;
    for (size_t i = 0; i < targets.items.size(); ++i) {
      const auto delta = WorldDelta(plan.offsets[i], plane, operation);
      for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(delta[axis]) <= 0.0001) continue;
        if (!std::isfinite(delta[axis]) || std::abs(delta[axis]) > std::numeric_limits<float>::max())
          return reject("Translation exceeds scene matrix precision.");
        kinds.emplace_back(Kind(targets.items[i].type));
        uuids.push_back(targets.items[i].uuid);
        components.emplace_back("position"); axes.emplace_back(1, "xyz"[axis]);
        values.push_back(delta[axis]); modes.emplace_back("relative"); spaces.emplace_back("world");
      }
    }
    if (values.empty()) return result;
    return transform::ExecuteBatch({transform::kBatchCommandId,
        {{"target_kinds", kinds}, {"target_uuids", uuids}, {"component_kinds", components},
         {"axes", axes}, {"values", values}, {"modes", modes}, {"spaces", spaces}}}, context);
  } catch (const std::exception &e) { return reject(e.what()); }
}
} // namespace alignment
