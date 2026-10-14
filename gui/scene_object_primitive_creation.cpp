#include "layer_identity.h"
#include "scene_object_primitive_creation.h"

#include "configmanager.h"
#include "layer.h"
#include "mvrscene.h"
#include "sceneobject.h"
#include "uuidutils.h"

#include <chrono>
#include <cmath>
#include <string>

#include <wx/string.h>

namespace scene_object_primitives {
namespace {
constexpr const char *kSceneObjectsLayerName = "3D Objects";
constexpr double kCylinderRoundToleranceMeters = 1e-6;

std::string EnsureCurrentLayerExists(MvrScene &scene, const std::string &layerName) {
  return layerdomain::EnsureLayerReference(scene, layerName);
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
    obj.uuid = GenerateUuid();
    if (quantity > 1)
      obj.name = baseName + " " + std::to_string(i + 1);
    else
      obj.name = baseName;
    layerdomain::AssignLayer(scene, obj, layerName);
    obj.geometries.push_back({primitiveToken, localTransform});
    scene.sceneObjects[obj.uuid] = obj;
  }
}

} // namespace

void AddSphereObjects(ConfigManager &cfg, const SphereRequest &request) {
  auto &scene = cfg.GetScene();
  const long long baseId = NextBaseId();
  const std::string layerName = kSceneObjectsLayerName;

  const auto layerUuid = EnsureCurrentLayerExists(scene, layerName);
  AddPrimitiveObjects(scene, layerUuid, request.name, "primitive:sphere",
                      BuildSphereScaleTransform(request.radiusMeters),
                      request.quantity, baseId);
}

void AddCubeObjects(ConfigManager &cfg, const CubeRequest &request) {
  auto &scene = cfg.GetScene();
  const long long baseId = NextBaseId();
  const std::string layerName = kSceneObjectsLayerName;

  const auto layerUuid = EnsureCurrentLayerExists(scene, layerName);
  AddPrimitiveObjects(scene, layerUuid, request.name, "primitive:cube",
                      BuildCubeScaleTransform(request.lengthMeters,
                                              request.heightMeters,
                                              request.widthMeters),
                      request.quantity, baseId);
}

void AddCylinderObjects(ConfigManager &cfg, const CylinderRequest &request) {
  auto &scene = cfg.GetScene();
  const long long baseId = NextBaseId();
  const std::string layerName = kSceneObjectsLayerName;

  EnsureCurrentLayerExists(scene, layerName);
  const bool isRoundCylinder =
      std::fabs(request.topRadiusMeters - request.bottomRadiusMeters) <
      kCylinderRoundToleranceMeters;
  const std::string primitiveToken =
      isRoundCylinder
          ? "primitive:cylinder"
          : BuildCylinderPrimitiveToken(request.topRadiusMeters,
                                        request.bottomRadiusMeters,
                                        request.heightMeters);
  const Matrix localTransform =
      isRoundCylinder
          ? BuildCylinderScaleTransform(request.topRadiusMeters, request.heightMeters)
          : Matrix{};
  AddPrimitiveObjects(scene, layerName, request.name, primitiveToken, localTransform,
                      request.quantity, baseId);
}

} // namespace scene_object_primitives
