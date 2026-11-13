/* This file is part of Perastage. */
#include "mvr_import_resource_resolver.h"
#include "filesystem_path_utils.h"
#include "mvrscene.h"
#include "uuidutils.h"

namespace mvr {
// Commit external selections to the same leased workspace as imported resources.
// One physical definition is copied once, independently of instance modes.
bool MvrImportResourceResolver::RetainSceneGdtfs(
    MvrScene &scene, std::string &diagnostic) const {
  namespace fs = std::filesystem;
  std::unordered_map<std::string, std::string> retained;
  for (auto &[uuid, fixture] : scene.fixtures) {
    if (fixture.gdtfSpec.empty()) continue;
    const auto source = ResolveScenePath(fixture.gdtfSpec);
    if (source.empty()) {
      diagnostic = "Unsafe selected GDTF reference: " + fixture.gdtfSpec;
      return false;
    }
    const auto relative = MakeSceneRelative(source);
    if (!PathUtils::PathFromUtf8(relative).is_absolute()) {
      fixture.gdtfSpec = relative;
      continue;
    }
    const auto key = GdtfIdentityKey(PathUtils::PathToUtf8(source));
    auto found = retained.find(key);
    if (found == retained.end()) {
      const auto destination = PathUtils::ResolveSceneResourcePath(
          "resolved-gdtf/" + GenerateUuid() + ".gdtf", sceneBasePath_);
      std::error_code error;
      if (!destination.empty()) fs::create_directories(destination.parent_path(), error);
      if (destination.empty() || error || !fs::copy_file(source, destination, error)) {
        diagnostic = "Could not retain selected GDTF '" + fixture.gdtfSpec + "': " + error.message();
        return false;
      }
      found = retained.emplace(key, MakeSceneRelative(destination)).first;
    }
    fixture.gdtfSpec = found->second;
  }
  return true;
}
} // namespace mvr
