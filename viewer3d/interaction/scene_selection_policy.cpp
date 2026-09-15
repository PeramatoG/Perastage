#include "scene_selection_policy.h"

#include "mvrscene.h"
#include "scene_grouping.h"

#include <algorithm>

namespace viewer3d::interaction {
namespace {

// Appends a non-empty UUID when it is not already in the target bucket.
void AppendUniqueUuid(std::vector<std::string> &uuids,
                      const std::string &uuid) {
  if (!uuid.empty() &&
      std::find(uuids.begin(), uuids.end(), uuid) == uuids.end()) {
    uuids.push_back(uuid);
  }
}

// Appends a UUID to the typed bucket owning its scene element.
void AppendTypedUuid(const MvrScene &scene, const std::string &uuid,
                     TypedSelection &selection) {
  if (scene.fixtures.contains(uuid))
    AppendUniqueUuid(selection.fixtures, uuid);
  else if (scene.trusses.contains(uuid))
    AppendUniqueUuid(selection.trusses, uuid);
  else if (scene.supports.contains(uuid))
    AppendUniqueUuid(selection.supports, uuid);
  else if (scene.sceneObjects.contains(uuid))
    AppendUniqueUuid(selection.sceneObjects, uuid);
}

} // namespace

// Builds typed table highlights for an item and the members of its group.
TypedSelection BuildHoverSelection(const MvrScene &scene,
                                   const std::string &hoverUuid) {
  TypedSelection selection;
  for (const auto &uuid :
       scene_grouping::ExpandHoverForGroupHighlights(scene, hoverUuid)) {
    AppendTypedUuid(scene, uuid, selection);
  }
  return selection;
}

// Builds the typed selection represented by clicking an item or its group.
TypedSelection BuildClickSelection(const MvrScene &scene,
                                   const std::string &clickedUuid) {
  TypedSelection selection;
  AppendTypedUuid(scene, clickedUuid, selection);
  for (const auto &uuid :
       scene_grouping::ExpandHoverForGroupHighlights(scene, clickedUuid)) {
    AppendTypedUuid(scene, uuid, selection);
  }
  return selection;
}

} // namespace viewer3d::interaction
