#include "../viewer3d/interaction/scene_selection_policy.h"

#include "mvrscene.h"
#include "scene_grouping.h"

#include <algorithm>
#include <cassert>

namespace {

// Reports whether a UUID occurs exactly once in a typed selection bucket.
bool OccursOnce(const std::vector<std::string> &uuids,
                const std::string &uuid) {
  return std::count(uuids.begin(), uuids.end(), uuid) == 1;
}

} // namespace

// Verifies direct and grouped hover/click selection classification.
int main() {
  MvrScene scene;
  scene.fixtures["fixture"].uuid = "fixture";
  scene.trusses["truss"].uuid = "truss";
  scene.supports["support"].uuid = "support";
  scene.sceneObjects["object"].uuid = "object";

  const auto direct =
      viewer3d::interaction::BuildClickSelection(scene, "fixture");
  assert(direct.fixtures == std::vector<std::string>{"fixture"});
  assert(direct.trusses.empty() && direct.supports.empty() &&
         direct.sceneObjects.empty());

  const scene_grouping::ObjectSelection groupSelection{
      .fixtures = {"fixture"},
      .trusses = {"truss"},
      .supports = {"support"},
      .sceneObjects = {"object"}};
  assert(scene_grouping::GroupSelection(scene, groupSelection).changed);

  const auto hover = viewer3d::interaction::BuildHoverSelection(scene, "truss");
  const auto click = viewer3d::interaction::BuildClickSelection(scene, "truss");
  assert(OccursOnce(hover.fixtures, "fixture"));
  assert(hover.trusses.empty());
  assert(OccursOnce(hover.supports, "support"));
  assert(OccursOnce(hover.sceneObjects, "object"));
  assert(OccursOnce(click.fixtures, "fixture"));
  assert(OccursOnce(click.trusses, "truss"));
  assert(OccursOnce(click.supports, "support"));
  assert(OccursOnce(click.sceneObjects, "object"));
}
