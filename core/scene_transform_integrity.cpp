#include "scene_transform_integrity.h"

#include "matrixutils.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace scene_transform_integrity {
namespace {

// Returns a stable display name for an MVR node type.
const char *TypeName(MvrNodeType type) {
  switch (type) {
  case MvrNodeType::Fixture: return "Fixture";
  case MvrNodeType::Truss: return "Truss";
  case MvrNodeType::Support: return "Support";
  case MvrNodeType::SceneObject: return "SceneObject";
  case MvrNodeType::GroupObject: return "GroupObject";
  }
  return "Unknown";
}

// Returns true when every component of a matrix is finite.
bool IsFinite(const Matrix &matrix) {
  for (const auto *axis : {&matrix.u, &matrix.v, &matrix.w, &matrix.o}) {
    for (float value : *axis) {
      if (!std::isfinite(value))
        return false;
    }
  }
  return true;
}

// Returns true when two complete matrices agree within the supplied tolerance.
bool NearlyEqual(const Matrix &lhs, const Matrix &rhs, float tolerance) {
  for (const auto &pair : {std::pair{&lhs.u, &rhs.u},
                           std::pair{&lhs.v, &rhs.v},
                           std::pair{&lhs.w, &rhs.w},
                           std::pair{&lhs.o, &rhs.o}}) {
    for (size_t index = 0; index < 3; ++index) {
      if (std::fabs((*pair.first)[index] - (*pair.second)[index]) > tolerance)
        return false;
    }
  }
  return true;
}

// Inverts an affine matrix and rejects singular linear components.
bool Invert(const Matrix &matrix, Matrix &inverse) {
  const float determinant =
      matrix.u[0] * (matrix.v[1] * matrix.w[2] - matrix.w[1] * matrix.v[2]) -
      matrix.v[0] * (matrix.u[1] * matrix.w[2] - matrix.w[1] * matrix.u[2]) +
      matrix.w[0] * (matrix.u[1] * matrix.v[2] - matrix.v[1] * matrix.u[2]);
  if (!std::isfinite(determinant) || std::fabs(determinant) < 1.0e-8f)
    return false;

  const float reciprocal = 1.0f / determinant;
  inverse = MatrixUtils::Identity();
  inverse.u = {(matrix.v[1] * matrix.w[2] - matrix.v[2] * matrix.w[1]) * reciprocal,
               (matrix.u[2] * matrix.w[1] - matrix.u[1] * matrix.w[2]) * reciprocal,
               (matrix.u[1] * matrix.v[2] - matrix.u[2] * matrix.v[1]) * reciprocal};
  inverse.v = {(matrix.v[2] * matrix.w[0] - matrix.v[0] * matrix.w[2]) * reciprocal,
               (matrix.u[0] * matrix.w[2] - matrix.u[2] * matrix.w[0]) * reciprocal,
               (matrix.u[2] * matrix.v[0] - matrix.u[0] * matrix.v[2]) * reciprocal};
  inverse.w = {(matrix.v[0] * matrix.w[1] - matrix.v[1] * matrix.w[0]) * reciprocal,
               (matrix.u[1] * matrix.w[0] - matrix.u[0] * matrix.w[1]) * reciprocal,
               (matrix.u[0] * matrix.v[1] - matrix.u[1] * matrix.v[0]) * reciprocal};
  const std::array<float, 3> translation = matrix.o;
  inverse.o = {
      -(inverse.u[0] * translation[0] + inverse.v[0] * translation[1] + inverse.w[0] * translation[2]),
      -(inverse.u[1] * translation[0] + inverse.v[1] * translation[1] + inverse.w[1] * translation[2]),
      -(inverse.u[2] * translation[0] + inverse.v[2] * translation[1] + inverse.w[2] * translation[2])};
  return IsFinite(inverse);
}

// Returns whether a referenced node exists with its declared type.
bool NodeExists(const MvrScene &scene, MvrNodeType type,
                const std::string &uuid) {
  switch (type) {
  case MvrNodeType::Fixture: return scene.fixtures.contains(uuid);
  case MvrNodeType::Truss: return scene.trusses.contains(uuid);
  case MvrNodeType::Support: return scene.supports.contains(uuid);
  case MvrNodeType::SceneObject: return scene.sceneObjects.contains(uuid);
  case MvrNodeType::GroupObject: return scene.groupObjects.contains(uuid);
  }
  return false;
}

// Returns the parent metadata stored by a referenced node.
std::string ParentUuid(const MvrScene &scene, MvrNodeType type,
                       const std::string &uuid) {
  switch (type) {
  case MvrNodeType::Fixture: return scene.fixtures.at(uuid).parentGroupUuid;
  case MvrNodeType::Truss: return scene.trusses.at(uuid).parentGroupUuid;
  case MvrNodeType::Support: return scene.supports.at(uuid).parentGroupUuid;
  case MvrNodeType::SceneObject: return scene.sceneObjects.at(uuid).parentGroupUuid;
  case MvrNodeType::GroupObject: return scene.groupObjects.at(uuid).parentGroupUuid;
  }
  return {};
}

// Appends a fatal diagnostic and updates the aggregate result.
void AddFatal(Result &result, MvrNodeType type, const std::string &uuid,
              const std::string &reason) {
  result.success = false;
  result.diagnostics.push_back({Severity::Fatal, type, uuid, reason, {}});
}

// Appends a deterministic repair diagnostic.
void AddRepair(Result &result, MvrNodeType type, const std::string &uuid,
               const std::string &reason) {
  result.repaired = true;
  result.diagnostics.push_back(
      {Severity::Repair, type, uuid, reason, "recalculated local transform from world transform"});
}

// Sorts diagnostics so map and child insertion order cannot affect output.
void SortDiagnostics(Result &result) {
  std::stable_sort(result.diagnostics.begin(), result.diagnostics.end(),
                   [](const Diagnostic &lhs, const Diagnostic &rhs) {
                     return std::tie(lhs.severity, lhs.type, lhs.uuid,
                                     lhs.reason, lhs.action) <
                            std::tie(rhs.severity, rhs.type, rhs.uuid,
                                     rhs.reason, rhs.action);
                   });
}

} // namespace

// Validates hierarchy transforms and repairs canonical local matrices safely.
Result ValidateAndRepair(MvrScene &scene, float tolerance) {
  Result result;
  using NodeKey = std::pair<MvrNodeType, std::string>;
  std::map<NodeKey, std::string> ownership;

  std::vector<std::string> groupUuids;
  for (const auto &[uuid, group] : scene.groupObjects)
    groupUuids.push_back(uuid);
  std::sort(groupUuids.begin(), groupUuids.end());

  for (const auto &groupUuid : groupUuids) {
    const GroupObject &group = scene.groupObjects.at(groupUuid);
    if (!group.parentGroupUuid.empty() &&
        !scene.groupObjects.contains(group.parentGroupUuid))
      AddFatal(result, MvrNodeType::GroupObject, groupUuid, "missing parent group");
    for (const auto &child : group.children) {
      if (!NodeExists(scene, child.type, child.uuid)) {
        AddFatal(result, child.type, child.uuid, "dangling child reference");
        continue;
      }
      const NodeKey key{child.type, child.uuid};
      const auto [it, inserted] = ownership.emplace(key, groupUuid);
      if (!inserted)
        AddFatal(result, child.type, child.uuid, "duplicate or conflicting group ownership");
      if (ParentUuid(scene, child.type, child.uuid) != groupUuid)
        AddFatal(result, child.type, child.uuid, "parent metadata disagrees with child reference");
    }
  }

  auto validateParentMetadata = [&](MvrNodeType type, const auto &nodes) {
    std::vector<std::string> uuids;
    for (const auto &[uuid, node] : nodes)
      uuids.push_back(uuid);
    std::sort(uuids.begin(), uuids.end());
    for (const auto &uuid : uuids) {
      const auto &node = nodes.at(uuid);
      if (node.parentGroupUuid.empty())
        continue;
      if (!scene.groupObjects.contains(node.parentGroupUuid))
        AddFatal(result, type, uuid, "missing parent group");
      else if (!ownership.contains({type, uuid}))
        AddFatal(result, type, uuid, "parent metadata has no matching child reference");
    }
  };
  validateParentMetadata(MvrNodeType::Fixture, scene.fixtures);
  validateParentMetadata(MvrNodeType::Truss, scene.trusses);
  validateParentMetadata(MvrNodeType::Support, scene.supports);
  validateParentMetadata(MvrNodeType::SceneObject, scene.sceneObjects);
  validateParentMetadata(MvrNodeType::GroupObject, scene.groupObjects);

  for (const auto &uuid : groupUuids) {
    std::set<std::string> visited;
    std::string current = uuid;
    while (!current.empty() && scene.groupObjects.contains(current)) {
      if (!visited.insert(current).second) {
        AddFatal(result, MvrNodeType::GroupObject, uuid, "group ancestry cycle");
        break;
      }
      current = scene.groupObjects.at(current).parentGroupUuid;
    }
  }

  auto validateFinite = [&](MvrNodeType type, const auto &nodes) {
    std::vector<std::string> uuids;
    for (const auto &[uuid, node] : nodes)
      uuids.push_back(uuid);
    std::sort(uuids.begin(), uuids.end());
    for (const auto &uuid : uuids) {
      const auto &node = nodes.at(uuid);
      if (!IsFinite(node.transform))
        AddFatal(result, type, uuid, "non-finite world transform");
      if (!node.parentGroupUuid.empty() && node.hasLocalTransform &&
          !IsFinite(node.localTransform))
        AddFatal(result, type, uuid, "non-finite local transform");
    }
  };
  validateFinite(MvrNodeType::Fixture, scene.fixtures);
  validateFinite(MvrNodeType::Truss, scene.trusses);
  validateFinite(MvrNodeType::Support, scene.supports);
  validateFinite(MvrNodeType::SceneObject, scene.sceneObjects);
  for (const auto &uuid : groupUuids) {
    const auto &group = scene.groupObjects.at(uuid);
    if (!IsFinite(group.transform))
      AddFatal(result, MvrNodeType::GroupObject, uuid, "non-finite world transform");
    if (!IsFinite(group.localTransform))
      AddFatal(result, MvrNodeType::GroupObject, uuid, "non-finite local transform");
    Matrix inverse;
    if (!group.children.empty() && IsFinite(group.transform) &&
        !Invert(group.transform, inverse))
      AddFatal(result, MvrNodeType::GroupObject, uuid,
               "non-invertible parent transform");
  }

  if (!result.success) {
    SortDiagnostics(result);
    return result;
  }

  auto repairLeaf = [&](MvrNodeType type, auto &nodes) {
    std::vector<std::string> uuids;
    for (const auto &[uuid, node] : nodes)
      uuids.push_back(uuid);
    std::sort(uuids.begin(), uuids.end());
    for (const auto &uuid : uuids) {
      auto &node = nodes.at(uuid);
      if (node.parentGroupUuid.empty())
        continue;
      Matrix inverse;
      const Matrix &parentWorld = scene.groupObjects.at(node.parentGroupUuid).transform;
      if (!Invert(parentWorld, inverse)) {
        AddFatal(result, type, uuid, "non-invertible parent transform");
        continue;
      }
      const Matrix expectedLocal = MatrixUtils::Multiply(inverse, node.transform);
      if (!node.hasLocalTransform ||
          !NearlyEqual(MatrixUtils::Multiply(parentWorld, node.localTransform),
                       node.transform, tolerance)) {
        AddRepair(result, type, uuid,
                  node.hasLocalTransform ? "stale local transform" : "missing local transform");
        node.localTransform = expectedLocal;
        node.hasLocalTransform = true;
      }
    }
  };
  repairLeaf(MvrNodeType::Fixture, scene.fixtures);
  repairLeaf(MvrNodeType::Truss, scene.trusses);
  repairLeaf(MvrNodeType::Support, scene.supports);
  repairLeaf(MvrNodeType::SceneObject, scene.sceneObjects);

  for (const auto &uuid : groupUuids) {
    auto &group = scene.groupObjects.at(uuid);
    Matrix expectedLocal = group.transform;
    if (!group.parentGroupUuid.empty()) {
      Matrix inverse;
      const Matrix &parentWorld = scene.groupObjects.at(group.parentGroupUuid).transform;
      if (!Invert(parentWorld, inverse)) {
        AddFatal(result, MvrNodeType::GroupObject, uuid, "non-invertible parent transform");
        continue;
      }
      expectedLocal = MatrixUtils::Multiply(inverse, group.transform);
    }
    const Matrix rebuilt = group.parentGroupUuid.empty()
                               ? group.localTransform
                               : MatrixUtils::Multiply(
                                     scene.groupObjects.at(group.parentGroupUuid).transform,
                                     group.localTransform);
    if (!NearlyEqual(rebuilt, group.transform, tolerance)) {
      AddRepair(result, MvrNodeType::GroupObject, uuid, "stale local transform");
      group.localTransform = expectedLocal;
    }
  }

  SortDiagnostics(result);
  return result;
}

// Formats one transform-integrity diagnostic for logs and user warnings.
std::string FormatDiagnostic(const Diagnostic &diagnostic) {
  std::ostringstream stream;
  stream << (diagnostic.severity == Severity::Fatal ? "fatal" : "repair")
         << ": " << TypeName(diagnostic.type) << " '" << diagnostic.uuid
         << "': " << diagnostic.reason;
  if (!diagnostic.action.empty())
    stream << "; " << diagnostic.action;
  return stream.str();
}

} // namespace scene_transform_integrity
