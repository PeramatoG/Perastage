#pragma once

#include <string>

class MvrScene;

namespace layerdomain {

enum class LayerStatus {
  Success,
  NoChange,
  ValidationFailure,
  NotFound,
  DuplicateName,
  InvalidUtf8,
  InvalidUuid,
  InvalidColor
};

struct LayerChangeSet {
  bool layerStructureChanged = false;
  bool layerAppearanceChanged = false;
  bool layerVisibilityChanged = false;
  bool currentLayerChanged = false;
  bool sceneContentChanged = false;
  bool selectionChanged = false;
};

struct LayerResult {
  LayerStatus status = LayerStatus::Success;
  std::string message;
  std::string layerUuid;
  std::string layerName;
  LayerChangeSet changes;
};

// Validates layer UUIDs, names, colors, and duplicate canonical names.
LayerResult ValidateSceneLayers(const MvrScene &scene);

// Converts a layer status to a stable English diagnostic.
std::string StatusMessage(LayerStatus status);

} // namespace layerdomain
