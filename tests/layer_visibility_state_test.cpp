#include "configservices.h"

#include <cassert>

int main() {
  LayerVisibilityState layers;
  MvrScene scene;

  layers.SetHiddenLayers({"Layer A"});
  assert(!layers.IsLayerVisible("Layer A"));
  assert(layers.IsLayerVisible("Layer B"));

  Layer layer;
  layer.uuid = "11111111-1111-1111-1111-111111111111";
  layer.name = "Layer A";
  scene.layers[layer.uuid] = layer;
  layers.SetLayerColor(scene, "Layer A", "#FF0000");
  assert(layers.GetLayerColor(scene, "Layer A").value() == "#FF0000");

  layers.SetCurrentLayer("Layer B");
  assert(layers.GetCurrentLayer() == "Layer B");

  auto names = layers.GetLayerNames(scene);
  assert(!names.empty());
  return 0;
}
