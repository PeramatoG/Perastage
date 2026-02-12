#include "configservices.h"

#include <cassert>

int main() {
  LayerVisibilityState layers;
  MvrScene scene;

  layers.SetHiddenLayers({"Layer A"});
  assert(!layers.IsLayerVisible("Layer A"));
  assert(layers.IsLayerVisible("Layer B"));

  layers.SetLayerColor(scene, "Layer A", "#ff0000");
  assert(layers.GetLayerColor(scene, "Layer A").value() == "#ff0000");

  layers.SetCurrentLayer("Layer B");
  assert(layers.GetCurrentLayer() == "Layer B");

  auto names = layers.GetLayerNames(scene);
  assert(!names.empty());
  return 0;
}
