#include "scene_object_primitive_editing.h"

#include "configmanager.h"
#include "scene_object_primitive_dialogs.h"
#include "sceneobject.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace scene_object_primitives {
namespace {

constexpr const char *kPrimitiveSphereToken = "primitive:sphere";
constexpr const char *kPrimitiveCubeToken = "primitive:cube";

float AxisLength(const std::array<float, 3> &axis) {
  return std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
}

std::string PrimaryPrimitiveToken(const SceneObject &object) {
  if (!object.geometries.empty())
    return object.geometries.front().modelFile;
  return object.modelFile;
}

bool IsEditablePrimitive(const SceneObject &object) {
  if (object.geometries.empty())
    return false;
  const std::string token = PrimaryPrimitiveToken(object);
  return token == kPrimitiveSphereToken || token == kPrimitiveCubeToken;
}

} // namespace

bool EditPrimitiveObjectByUuid(wxWindow *parent, ConfigManager &cfg,
                               const std::string &uuid) {
  auto &scene = cfg.GetScene();
  auto it = scene.sceneObjects.find(uuid);
  if (it == scene.sceneObjects.end())
    return false;

  SceneObject &object = it->second;
  if (!IsEditablePrimitive(object))
    return false;

  const std::string token = PrimaryPrimitiveToken(object);
  const Matrix currentTransform = object.geometries.front().localTransform;

  Matrix updatedTransform = currentTransform;
  bool accepted = false;

  if (token == kPrimitiveSphereToken) {
    SphereRequest request;
    const double uniformScale = std::max(
        {static_cast<double>(AxisLength(currentTransform.u)),
         static_cast<double>(AxisLength(currentTransform.v)),
         static_cast<double>(AxisLength(currentTransform.w)), 0.01});
    request.radiusMeters = uniformScale * 0.5;
    accepted = ShowSphereEditDialog(parent, request);
    if (!accepted)
      return false;
    updatedTransform = BuildSphereScaleTransform(request.radiusMeters);
  } else if (token == kPrimitiveCubeToken) {
    CubeRequest request;
    request.lengthMeters =
        std::max(static_cast<double>(AxisLength(currentTransform.u)), 0.01);
    request.heightMeters =
        std::max(static_cast<double>(AxisLength(currentTransform.v)), 0.01);
    request.widthMeters =
        std::max(static_cast<double>(AxisLength(currentTransform.w)), 0.01);
    accepted = ShowCubeEditDialog(parent, request);
    if (!accepted)
      return false;
    updatedTransform = BuildCubeScaleTransform(request.lengthMeters,
                                               request.heightMeters,
                                               request.widthMeters);
  }

  if (!accepted ||
      (updatedTransform.u == currentTransform.u &&
       updatedTransform.v == currentTransform.v &&
       updatedTransform.w == currentTransform.w))
    return false;

  cfg.PushUndoState("edit primitive geometry");
  object.geometries.front().localTransform = updatedTransform;

  return true;
}

} // namespace scene_object_primitives
