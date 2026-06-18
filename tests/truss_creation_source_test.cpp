#include <cassert>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "truss_creation_source.h"

namespace fs = std::filesystem;

// Verifies that scene instances collapse into reusable truss-type entries.
int main() {
  std::unordered_map<std::string, Truss> trusses;

  Truss first;
  first.name = "FK40Q H-300 1";
  first.model = "FK40Q H-300";
  first.gdtfSpec = "trusses/FK40Q_H-300.gdtf";
  first.symbolFile = "cache/main.svg";
  trusses.emplace("first", first);

  Truss second = first;
  second.name = "FK40Q H-300 2";
  trusses.emplace("second", second);

  Truss third = first;
  third.name = "FK40Q H-300 3";
  third.parentGroupUuid = "bridge";
  trusses.emplace("third", third);

  Truss directModel;
  directModel.name = "Custom truss 1";
  directModel.model = "Custom truss";
  directModel.modelFile = "models/custom.glb";
  directModel.symbolFile = "cache/custom.svg";
  trusses.emplace("direct", directModel);

  Truss unusable;
  unusable.name = "Unusable";
  unusable.model = "Unusable type";
  unusable.symbolFile = "cache/unusable.svg";
  trusses.emplace("unusable", unusable);

  const fs::path sceneBase = fs::path("projects") / "show";
  const auto sources =
      gui::CollectTrussCreationSources(trusses, sceneBase.generic_string());

  assert(sources.size() == 2);
  assert(sources[0].displayName == "Custom truss");
  assert(fs::path(sources[0].definitionPath) ==
         sceneBase / "models" / "custom.glb");
  assert(sources[1].displayName == "FK40Q H-300");
  assert(fs::path(sources[1].definitionPath) ==
         sceneBase / "trusses" / "FK40Q_H-300.gdtf");
}
