#include "scene_object_primitive_editing.h"

#include "configmanager.h"
#include "scene_object_primitive_dialogs.h"
#include "sceneobject.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace scene_object_primitives {
namespace {

constexpr const char *kPrimitiveSphereToken = "primitive:sphere";
constexpr const char *kPrimitiveCubeToken = "primitive:cube";

enum class PrimitiveKind {
  None,
  Sphere,
  Cube,
};

struct PrimitiveGeometryTarget {
  PrimitiveKind kind = PrimitiveKind::None;
  size_t geometryIndex = 0;
  bool usesGeometryEntry = false;
};

float AxisLength(const std::array<float, 3> &axis) {
  return std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
}

std::string ToLowerTrimmed(std::string value) {
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(), [](unsigned char c) {
                return !std::isspace(c);
              }));
  value.erase(std::find_if(value.rbegin(), value.rend(),
                           [](unsigned char c) { return !std::isspace(c); })
                  .base(),
              value.end());
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

PrimitiveKind PrimitiveKindFromToken(const std::string &token) {
  const std::string normalized = ToLowerTrimmed(token);
  if (normalized.rfind(kPrimitiveSphereToken, 0) == 0)
    return PrimitiveKind::Sphere;
  if (normalized.rfind(kPrimitiveCubeToken, 0) == 0)
    return PrimitiveKind::Cube;
  return PrimitiveKind::None;
}

PrimitiveGeometryTarget ResolvePrimitiveTarget(const SceneObject &object) {
  for (size_t i = 0; i < object.geometries.size(); ++i) {
    const PrimitiveKind kind =
        PrimitiveKindFromToken(object.geometries[i].modelFile);
    if (kind != PrimitiveKind::None) {
      PrimitiveGeometryTarget target;
      target.kind = kind;
      target.geometryIndex = i;
      target.usesGeometryEntry = true;
      return target;
    }
  }

  const PrimitiveKind modelKind = PrimitiveKindFromToken(object.modelFile);
  if (modelKind != PrimitiveKind::None) {
    PrimitiveGeometryTarget target;
    target.kind = modelKind;
    target.geometryIndex = 0;
    target.usesGeometryEntry = false;
    return target;
  }

  return {};
}

Matrix ResolveEditableTransform(const SceneObject &object,
                               const PrimitiveGeometryTarget &target) {
  if (target.usesGeometryEntry && target.geometryIndex < object.geometries.size())
    return object.geometries[target.geometryIndex].localTransform;
  return Matrix{};
}

} // namespace

bool EditPrimitiveObjectByUuid(wxWindow *parent, ConfigManager &cfg,
                               const std::string &uuid) {
  auto &scene = cfg.GetScene();
  auto it = scene.sceneObjects.find(uuid);
  if (it == scene.sceneObjects.end())
    return false;

  SceneObject &object = it->second;
  const PrimitiveGeometryTarget target = ResolvePrimitiveTarget(object);
  if (target.kind == PrimitiveKind::None)
    return false;

  const Matrix currentTransform = ResolveEditableTransform(object, target);

  Matrix updatedTransform = currentTransform;
  bool accepted = false;

  if (target.kind == PrimitiveKind::Sphere) {
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
  } else if (target.kind == PrimitiveKind::Cube) {
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
  if (target.usesGeometryEntry && target.geometryIndex < object.geometries.size()) {
    object.geometries[target.geometryIndex].localTransform = updatedTransform;
  } else {
    GeometryInstance geometry;
    geometry.modelFile = object.modelFile;
    geometry.localTransform = updatedTransform;
    object.geometries.push_back(geometry);
  }

  return true;
}

} // namespace scene_object_primitives
