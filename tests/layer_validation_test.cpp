#include "layer_validation.h"
#include "mvrscene.h"

#include <cassert>
#include <string>

// Verifies neutral layer validation for valid identities, colors, and names.
int main() {
  MvrScene scene;
  Layer valid;
  valid.uuid = "f806f881-7e6d-538f-bc17-e2a08ec678f1";
  valid.name = "Lighting";
  valid.color = "#AABBCC";
  scene.layers[valid.uuid] = valid;
  assert(layerdomain::ValidateSceneLayers(scene).status ==
         layerdomain::LayerStatus::Success);

  scene.layers.begin()->second.color = "#aabbcc";
  assert(layerdomain::ValidateSceneLayers(scene).status ==
         layerdomain::LayerStatus::InvalidColor);
  scene.layers.begin()->second.color.clear();
  scene.layers.begin()->second.name = std::string("Bad ") + char(0xFF);
  assert(layerdomain::ValidateSceneLayers(scene).status ==
         layerdomain::LayerStatus::InvalidUtf8);

  scene.layers.clear();
  valid.uuid = "not-a-uuid";
  valid.name = "Lighting";
  scene.layers[valid.uuid] = valid;
  assert(layerdomain::ValidateSceneLayers(scene).status ==
         layerdomain::LayerStatus::InvalidUuid);
  assert(layerdomain::StatusMessage(layerdomain::LayerStatus::DuplicateName) ==
         "Layer name already exists.");
  return 0;
}
