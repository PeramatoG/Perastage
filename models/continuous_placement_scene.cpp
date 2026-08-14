#include "continuous_placement_scene.h"

#include "mvrscene.h"

#include <utility>

namespace continuous_placement {

// Reports whether the requested scene element exists.
bool Contains(const MvrScene &scene, ContinuousPlacementType type,
              const std::string &uuid) {
  switch (type) {
  case ContinuousPlacementType::Fixture:
    return scene.fixtures.contains(uuid);
  case ContinuousPlacementType::Truss:
    return scene.trusses.contains(uuid);
  case ContinuousPlacementType::Support:
    return scene.supports.contains(uuid);
  case ContinuousPlacementType::SceneObject:
    return scene.sceneObjects.contains(uuid);
  case ContinuousPlacementType::None:
    return false;
  }
  return false;
}

// Clones a supported scene element with a new UUID.
bool CloneElement(MvrScene &scene, ContinuousPlacementType type,
                  const std::string &sourceUuid, const std::string &nextUuid) {
  switch (type) {
  case ContinuousPlacementType::Fixture: {
    const auto it = scene.fixtures.find(sourceUuid);
    if (it == scene.fixtures.end())
      return false;
    Fixture next = it->second;
    next.uuid = nextUuid;
    next.fixtureId += 1;
    scene.fixtures[nextUuid] = std::move(next);
    return true;
  }
  case ContinuousPlacementType::Truss: {
    const auto it = scene.trusses.find(sourceUuid);
    if (it == scene.trusses.end())
      return false;
    Truss next = it->second;
    next.uuid = nextUuid;
    scene.trusses[nextUuid] = std::move(next);
    return true;
  }
  case ContinuousPlacementType::Support: {
    const auto it = scene.supports.find(sourceUuid);
    if (it == scene.supports.end())
      return false;
    Support next = it->second;
    next.uuid = nextUuid;
    scene.supports[nextUuid] = std::move(next);
    return true;
  }
  case ContinuousPlacementType::SceneObject: {
    const auto it = scene.sceneObjects.find(sourceUuid);
    if (it == scene.sceneObjects.end())
      return false;
    SceneObject next = it->second;
    next.uuid = nextUuid;
    scene.sceneObjects[nextUuid] = std::move(next);
    return true;
  }
  case ContinuousPlacementType::None:
    return false;
  }
  return false;
}

// Removes a supported scene element by UUID.
void EraseElement(MvrScene &scene, ContinuousPlacementType type,
                  const std::string &uuid) {
  switch (type) {
  case ContinuousPlacementType::Fixture:
    scene.fixtures.erase(uuid);
    break;
  case ContinuousPlacementType::Truss:
    scene.trusses.erase(uuid);
    break;
  case ContinuousPlacementType::Support:
    scene.supports.erase(uuid);
    break;
  case ContinuousPlacementType::SceneObject:
    scene.sceneObjects.erase(uuid);
    break;
  case ContinuousPlacementType::None:
    break;
  }
}

// Returns the element origin in viewer world units.
std::array<float, 3> PositionMeters(const MvrScene &scene,
                                    ContinuousPlacementType type,
                                    const std::string &uuid) {
  switch (type) {
  case ContinuousPlacementType::Fixture: {
    const auto it = scene.fixtures.find(uuid);
    if (it != scene.fixtures.end())
      return {it->second.transform.o[0] / 1000.0f,
              it->second.transform.o[1] / 1000.0f,
              it->second.transform.o[2] / 1000.0f};
    break;
  }
  case ContinuousPlacementType::Truss: {
    const auto it = scene.trusses.find(uuid);
    if (it != scene.trusses.end())
      return {it->second.transform.o[0] / 1000.0f,
              it->second.transform.o[1] / 1000.0f,
              it->second.transform.o[2] / 1000.0f};
    break;
  }
  case ContinuousPlacementType::Support: {
    const auto it = scene.supports.find(uuid);
    if (it != scene.supports.end())
      return {it->second.transform.o[0] / 1000.0f,
              it->second.transform.o[1] / 1000.0f,
              it->second.transform.o[2] / 1000.0f};
    break;
  }
  case ContinuousPlacementType::SceneObject: {
    const auto it = scene.sceneObjects.find(uuid);
    if (it != scene.sceneObjects.end())
      return {it->second.transform.o[0] / 1000.0f,
              it->second.transform.o[1] / 1000.0f,
              it->second.transform.o[2] / 1000.0f};
    break;
  }
  case ContinuousPlacementType::None:
    break;
  }
  return {0.0f, 0.0f, 0.0f};
}

// Sets the element origin from viewer world units without changing orientation.
void SetPositionMeters(MvrScene &scene, ContinuousPlacementType type,
                       const std::string &uuid,
                       const std::array<float, 3> &positionMeters) {
  auto setOrigin = [&](auto &elements) {
    const auto it = elements.find(uuid);
    if (it != elements.end())
      it->second.transform.o = {positionMeters[0] * 1000.0f,
                                positionMeters[1] * 1000.0f,
                                positionMeters[2] * 1000.0f};
  };
  switch (type) {
  case ContinuousPlacementType::Fixture:
    setOrigin(scene.fixtures);
    break;
  case ContinuousPlacementType::Truss:
    setOrigin(scene.trusses);
    break;
  case ContinuousPlacementType::Support:
    setOrigin(scene.supports);
    break;
  case ContinuousPlacementType::SceneObject:
    setOrigin(scene.sceneObjects);
    break;
  case ContinuousPlacementType::None:
    break;
  }
}

// Returns a user-facing lowercase name for the placement element type.
const char *ElementName(ContinuousPlacementType type) {
  switch (type) {
  case ContinuousPlacementType::Fixture:
    return "fixture";
  case ContinuousPlacementType::Truss:
    return "truss";
  case ContinuousPlacementType::Support:
    return "support";
  case ContinuousPlacementType::SceneObject:
    return "scene object";
  case ContinuousPlacementType::None:
    return "element";
  }
  return "element";
}

} // namespace continuous_placement
