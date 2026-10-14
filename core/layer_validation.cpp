#include "layer_validation.h"

#include "mvrscene.h"
#include "utf8_utils.h"
#include "uuidutils.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <set>

namespace layerdomain {
namespace {

// Returns true when the color is a canonical #RRGGBB value.
bool IsCanonicalColor(const std::string &color) {
  if (color.empty())
    return true;
  if (color.size() != 7 || color[0] != '#')
    return false;
  return std::all_of(color.begin() + 1, color.end(), [](unsigned char value) {
    return std::isxdigit(value) != 0 && !std::islower(value);
  });
}

} // namespace

// Validates layer UUIDs, authored UTF-8 names, colors, and membership.
LayerResult ValidateSceneLayers(const MvrScene &scene) {
  std::set<std::string> uuids;
  for (const auto &[uuid, layer] : scene.layers) {
    if (CanonicalizeUuid(uuid) != uuid || uuid.empty() || layer.uuid != uuid)
      return {LayerStatus::InvalidUuid, "Layer UUID is malformed", uuid,
              layer.name};
    if (!uuids.insert(uuid).second)
      return {LayerStatus::InvalidUuid, "Layer UUID is duplicated", uuid,
              layer.name};
    if (!IsValidUtf8(layer.name))
      return {LayerStatus::InvalidUtf8, "Layer name is not valid UTF-8", uuid};
    if (!IsCanonicalColor(layer.color))
      return {LayerStatus::InvalidColor, "Layer color is invalid", uuid,
              layer.name};
  }
  auto validMembership = [&](const auto &objects) {
    for (const auto &[uuid, object] : objects)
      if (object.layerUuid.empty() || !scene.layers.contains(object.layerUuid))
        return false;
    return true;
  };
  if (!validMembership(scene.fixtures) || !validMembership(scene.trusses) ||
      !validMembership(scene.supports) || !validMembership(scene.sceneObjects) ||
      !validMembership(scene.groupObjects))
    return {LayerStatus::InvalidUuid, "Layer membership is unresolved"};
  return {};
}

// Converts a layer status to a stable English diagnostic.
std::string StatusMessage(LayerStatus status) {
  switch (status) {
  case LayerStatus::Success: return "Layer operation succeeded.";
  case LayerStatus::NoChange: return "Layer operation made no changes.";
  case LayerStatus::ValidationFailure: return "Layer data is not valid.";
  case LayerStatus::NotFound: return "Layer was not found.";
  case LayerStatus::DuplicateName: return "Layer name already exists.";
  case LayerStatus::InvalidUtf8: return "Layer text is not valid UTF-8.";
  case LayerStatus::InvalidUuid: return "Layer UUID is not valid.";
  case LayerStatus::InvalidColor: return "Layer color is not valid.";
  }
  return "Layer operation failed.";
}

} // namespace layerdomain
