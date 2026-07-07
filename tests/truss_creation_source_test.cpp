#include <cassert>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "truss_creation_source.h"

namespace fs = std::filesystem;

// Finds a creation source by display name and definition path suffix.
static const gui::TrussCreationSource *FindSource(
    const std::vector<gui::TrussCreationSource> &sources,
    const std::string &displayName, const fs::path &definitionPath) {
  for (const auto &source : sources) {
    if (source.displayName == displayName &&
        fs::path(source.definitionPath) == definitionPath)
      return &source;
  }
  return nullptr;
}

// Verifies that scene instances collapse into reusable truss-type entries.
int main() {
  std::unordered_map<std::string, Truss> trusses;
  const fs::path sceneBase = fs::path("projects") / "show";

  Truss gdtf;
  gdtf.name = "FK40Q H-300 1";
  gdtf.model = "FK40Q H-300";
  gdtf.gdtfSpec = "trusses/FK40Q_H-300.gdtf";
  gdtf.symbolFile = "cache/main.svg";
  trusses.emplace("gdtf", gdtf);

  Truss directNamed;
  directNamed.name = "Custom truss 1";
  directNamed.model = "Internal model should not win";
  directNamed.modelFile = "models/custom.glb";
  directNamed.perastageTypeKey = "type\x1fserialized{matrix}";
  trusses.emplace("directNamed", directNamed);

  Truss directNamedDuplicate = directNamed;
  trusses.emplace("directNamedDuplicate", directNamedDuplicate);

  Truss directStem;
  directStem.modelFile = "models/stem_name.3ds";
  directStem.perastageTypeKey = "bad\x1fkey";
  trusses.emplace("directStem", directStem);

  Truss sameNameDifferentPathA;
  sameNameDifferentPathA.name = "Shared label";
  sameNameDifferentPathA.modelFile = "models/a.glb";
  trusses.emplace("sameNameA", sameNameDifferentPathA);

  Truss sameNameDifferentPathB = sameNameDifferentPathA;
  sameNameDifferentPathB.modelFile = "models/b.glb";
  trusses.emplace("sameNameB", sameNameDifferentPathB);

  Truss unusable;
  unusable.name = "Unusable";
  unusable.model = "Unusable type";
  unusable.symbolFile = "cache/unusable.svg";
  trusses.emplace("unusable", unusable);

  const auto sources =
      gui::CollectTrussCreationSources(trusses, sceneBase.generic_string());

  assert(sources.size() == 5);
  assert(FindSource(sources, "FK40Q H-300",
                    sceneBase / "trusses" / "FK40Q_H-300.gdtf"));
  assert(FindSource(sources, "Custom truss 1",
                    sceneBase / "models" / "custom.glb"));
  assert(FindSource(sources, "stem_name",
                    sceneBase / "models" / "stem_name.3ds"));
  assert(FindSource(sources, "Shared label", sceneBase / "models" / "a.glb"));
  assert(FindSource(sources, "Shared label", sceneBase / "models" / "b.glb"));

  for (const auto &source : sources) {
    assert(source.displayName.find('\x1f') == std::string::npos);
    assert(source.displayName.find("serialized") == std::string::npos);
    assert(!source.identityKey.empty());
    assert(!source.definitionPath.empty());
  }
}
