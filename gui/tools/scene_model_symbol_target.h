#pragma once

#include <string>

namespace tools {

enum class SceneModelKind {
  Fixture,
  Truss,
  SceneObject,
};

struct SceneModelSymbolTarget {
  SceneModelKind kind = SceneModelKind::Fixture;
  std::string uuid;
};

} // namespace tools
