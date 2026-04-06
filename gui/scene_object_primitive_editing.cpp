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
constexpr const char *kPrimitiveCylinderToken = "primitive:cylinder";
constexpr float kScreenDepthMeters = 0.1f;

enum class PrimitiveKind {
  None,
  Sphere,
  Cube,
  Cylinder,
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
  if (normalized.rfind(kPrimitiveCylinderToken, 0) == 0)
    return PrimitiveKind::Cylinder;
  return PrimitiveKind::None;
}
std::array<float, 3> AxisWithLength(const std::array<float, 3> &axis,
                                    float length) {
  const float current = AxisLength(axis);
  if (current < 1e-6f)
    return {length, 0.0f, 0.0f};
  const float scale = length / current;
  return {axis[0] * scale, axis[1] * scale, axis[2] * scale};
}

bool IsScreenObject(const SceneObject &object) {
  const std::string name = ToLowerTrimmed(object.name);
  return name.find("screen") != std::string::npos ||
         name.find("pantalla") != std::string::npos;
}

bool IsPipeObject(const SceneObject &object) {
  const std::string name = ToLowerTrimmed(object.name);
  return name.rfind("pipe", 0) == 0;
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

  if (IsScreenObject(object)) {
    ScreenEditRequest request;
    request.widthMeters = std::max(0.01, static_cast<double>(AxisLength(object.transform.u)));
    request.heightMeters = std::max(0.01, static_cast<double>(AxisLength(object.transform.w)));
    if (!ShowScreenEditDialog(parent, request))
      return false;

    const auto oldU = object.transform.u;
    const auto oldV = object.transform.v;
    const auto oldW = object.transform.w;
    object.transform.u = AxisWithLength(object.transform.u, static_cast<float>(request.widthMeters));
    object.transform.v = AxisWithLength(object.transform.v, kScreenDepthMeters);
    object.transform.w = AxisWithLength(object.transform.w, static_cast<float>(request.heightMeters));

    if (object.transform.u == oldU && object.transform.v == oldV && object.transform.w == oldW)
      return false;
    cfg.PushUndoState("edit screen geometry");
    return true;
  }

  if (IsPipeObject(object)) {
    PipeEditRequest request;
    request.lengthMeters = std::max(0.01, static_cast<double>(AxisLength(object.transform.u)));
    if (!ShowPipeEditDialog(parent, request))
      return false;

    const auto oldU = object.transform.u;
    object.transform.u = AxisWithLength(object.transform.u, static_cast<float>(request.lengthMeters));
    if (object.transform.u == oldU)
      return false;
    cfg.PushUndoState("edit pipe geometry");
    return true;
  }

  const PrimitiveGeometryTarget target = ResolvePrimitiveTarget(object);
  if (target.kind == PrimitiveKind::None)
    return false;

  const Matrix currentTransform = ResolveEditableTransform(object, target);

  Matrix updatedTransform = currentTransform;
  bool accepted = false;
  bool primitiveTokenUpdated = false;

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
  } else if (target.kind == PrimitiveKind::Cylinder) {
    CylinderRequest request = ParseCylinderPrimitiveToken(
        target.usesGeometryEntry && target.geometryIndex < object.geometries.size()
            ? object.geometries[target.geometryIndex].modelFile
            : object.modelFile,
        currentTransform);
    accepted = ShowCylinderEditDialog(parent, request);
    if (!accepted)
      return false;
    updatedTransform = Matrix{};
    if (target.usesGeometryEntry && target.geometryIndex < object.geometries.size()) {
      object.geometries[target.geometryIndex].modelFile =
          BuildCylinderPrimitiveToken(request.topRadiusMeters,
                                      request.bottomRadiusMeters,
                                      request.heightMeters);
    } else {
      object.modelFile = BuildCylinderPrimitiveToken(request.topRadiusMeters,
                                                     request.bottomRadiusMeters,
                                                     request.heightMeters);
    }
    primitiveTokenUpdated = true;
  }

  if (!accepted ||
      (!primitiveTokenUpdated &&
      (updatedTransform.u == currentTransform.u &&
       updatedTransform.v == currentTransform.v &&
       updatedTransform.w == currentTransform.w)))
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
