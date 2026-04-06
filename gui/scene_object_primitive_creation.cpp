#include "scene_object_primitive_creation.h"

#include "configmanager.h"
#include "layer.h"
#include "mvrscene.h"
#include "sceneobject.h"

#include <chrono>
#include <string>

#include <wx/string.h>

namespace scene_object_primitives {
namespace {

void EnsureCurrentLayerExists(MvrScene &scene, const std::string &layerName,
                              long long baseId) {
  for (const auto &[uid, layer] : scene.layers) {
    if (layer.name == layerName)
      return;
  }

  Layer layer;
  layer.uuid = wxString::Format("layer_%lld", baseId).ToStdString();
  layer.name = layerName;
  scene.layers[layer.uuid] = layer;
}

long long NextBaseId() {
  return static_cast<long long>(
      std::chrono::steady_clock::now().time_since_epoch().count());
}

void AddPrimitiveObjects(MvrScene &scene, const std::string &layerName,
                         const std::string &baseName,
                         const std::string &primitiveToken,
                         const Matrix &localTransform, int quantity,
                         long long baseId) {
  for (int i = 0; i < quantity; ++i) {
    SceneObject obj;
    obj.uuid = wxString::Format("uuid_%lld", baseId + i).ToStdString();
    if (quantity > 1)
      obj.name = baseName + " " + std::to_string(i + 1);
    else
      obj.name = baseName;
    obj.layer = layerName;
    obj.geometries.push_back({primitiveToken, localTransform});
    scene.sceneObjects[obj.uuid] = obj;
  }
}

} // namespace

void AddSphereObjects(ConfigManager &cfg, const SphereRequest &request) {
  auto &scene = cfg.GetScene();
  const long long baseId = NextBaseId();
  const std::string layerName = cfg.GetCurrentLayer();

  EnsureCurrentLayerExists(scene, layerName, baseId);
  AddPrimitiveObjects(scene, layerName, "Sphere", "primitive:sphere",
                      BuildSphereScaleTransform(request.radiusMeters),
                      request.quantity, baseId);
}

void AddCubeObjects(ConfigManager &cfg, const CubeRequest &request) {
  auto &scene = cfg.GetScene();
  const long long baseId = NextBaseId();
  const std::string layerName = cfg.GetCurrentLayer();

  EnsureCurrentLayerExists(scene, layerName, baseId);
  AddPrimitiveObjects(scene, layerName, "Cube", "primitive:cube",
                      BuildCubeScaleTransform(request.lengthMeters,
                                              request.heightMeters,
                                              request.widthMeters),
                      request.quantity, baseId);
}

void AddCylinderObjects(ConfigManager &cfg, const CylinderRequest &request) {
  auto &scene = cfg.GetScene();
  const long long baseId = NextBaseId();
  const std::string layerName = cfg.GetCurrentLayer();

  EnsureCurrentLayerExists(scene, layerName, baseId);
  AddPrimitiveObjects(scene, layerName, "Cylinder",
                      BuildCylinderPrimitiveToken(request.topRadiusMeters,
                                                  request.bottomRadiusMeters,
                                                  request.heightMeters),
                      Matrix{}, request.quantity, baseId);
}

} // namespace scene_object_primitives
