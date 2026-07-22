#include "layer_service.h"
#include "utf8_utils.h"

#include <cassert>
#include <string>

namespace {

// Builds a minimal scene with one layer and one support assigned to it.
MvrScene MakeSceneWithSupportLayer(const std::string &uuid,
                                   const std::string &name) {
  MvrScene scene;
  Layer layer;
  layer.uuid = uuid;
  layer.name = name;
  scene.layers[uuid] = layer;
  Support support;
  support.uuid = "support-1";
  support.layer = name;
  scene.supports[support.uuid] = support;
  return scene;
}

} // namespace

// Verifies UTF-8 validation and controlled Windows-1252 repair.
int main() {
  assert(IsValidUtf8("Iluminación"));
  assert(IsValidUtf8("Vídeo"));
  assert(IsValidUtf8("Música"));
  assert(IsValidUtf8("照明"));

  const std::string invalid = std::string("rig Iluminaci") + char(0xF3) + "n";
  assert(!IsValidUtf8(invalid));
  const auto repaired = RepairWindows1252AsUtf8(invalid);
  assert(repaired.has_value());
  assert(*repaired == "rig Iluminación");
  assert(RepairWindows1252AsUtf8("rig Iluminación").value() ==
         "rig Iluminación");

  MvrScene scene = MakeSceneWithSupportLayer(
      "f806f881-7e6d-538f-bc17-e2a08ec678f1", invalid);
  Layer valid;
  valid.uuid = "1e4e210c-6fe9-579c-a6f5-e4ba379b368e";
  valid.name = "rig Iluminación";
  scene.layers[valid.uuid] = valid;

  const auto reconcile = layerdomain::ReconcileLegacyLayers(scene);
  assert(reconcile.status == layerdomain::LayerStatus::Success);
  assert(layerdomain::ValidateSceneLayers(scene).status ==
         layerdomain::LayerStatus::Success);
  assert(scene.layers.at("f806f881-7e6d-538f-bc17-e2a08ec678f1").name ==
         "rig Iluminación (Recovered 2)");
  assert(scene.layers.at("1e4e210c-6fe9-579c-a6f5-e4ba379b368e").name ==
         "rig Iluminación");

  const auto entries = layerdomain::EnumerateLayers(scene);
  bool foundSupportLayer = false;
  for (const auto &entry : entries) {
    if (entry.name == "rig Iluminación (Recovered 2)")
      foundSupportLayer = true;
  }
  assert(foundSupportLayer);

  const auto second = layerdomain::ReconcileLegacyLayers(scene);
  assert(second.status == layerdomain::LayerStatus::NoChange);
  assert(IsValidUtf8("Vídeo"));
  assert(IsValidUtf8("照明"));
  return 0;
}
