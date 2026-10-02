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

// Trims ASCII whitespace without changing interior layer text.
std::string TrimLayerName(const std::string &name) {
  auto isSpace = [](unsigned char value) { return std::isspace(value) != 0; };
  size_t first = 0;
  size_t last = name.size();
  while (first < last && isSpace(static_cast<unsigned char>(name[first])))
    ++first;
  while (last > first && isSpace(static_cast<unsigned char>(name[last - 1])))
    --last;
  return name.substr(first, last - first);
}

} // namespace

// Validates layer UUIDs, names, colors, and duplicate canonical names.
LayerResult ValidateSceneLayers(const MvrScene &scene) {
  std::set<std::string> names;
  std::set<std::string> uuids;
  for (const auto &[uuid, layer] : scene.layers) {
    if (CanonicalizeUuid(uuid).empty())
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
    if (!names.insert(TrimLayerName(layer.name)).second)
      return {LayerStatus::DuplicateName, "Layer name is duplicated", uuid,
              layer.name};
  }
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
