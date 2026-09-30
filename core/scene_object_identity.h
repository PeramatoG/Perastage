#pragma once

#include <string>

namespace perastage::scene_identity {

// Identifies the stable semantic category of a scene object.
enum class ObjectKind { Fixture, Truss, Support, SceneObject, Group };

struct ObjectReference {
  ObjectKind kind = ObjectKind::Fixture;
  std::string uuid;

  bool operator==(const ObjectReference &) const = default;
};

// Returns the stable technical token for an object kind.
inline const char *KindToken(ObjectKind kind) {
  switch (kind) {
  case ObjectKind::Fixture:
    return "fixture";
  case ObjectKind::Truss:
    return "truss";
  case ObjectKind::Support:
    return "support";
  case ObjectKind::SceneObject:
    return "scene_object";
  case ObjectKind::Group:
    return "group";
  }
  return "invalid";
}

} // namespace perastage::scene_identity
