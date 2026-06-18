#include "truss_creation_source.h"

#include <filesystem>
#include <map>

#include "filesystem_path_utils.h"
#include "trussloader.h"

namespace gui {
namespace {

// Returns the stable type label shown when reusing a truss from the scene.
std::string GetTrussTypeDisplayName(const Truss &truss) {
  if (!truss.model.empty())
    return truss.model;
  if (!truss.perastageTypeKey.empty())
    return truss.perastageTypeKey;
  return truss.name;
}

// Resolves a supported truss definition reference against the scene directory.
std::string ResolveDefinitionPath(const std::string &reference,
                                  const std::filesystem::path &sceneBasePath) {
  if (reference.empty() || !IsSupportedTrussDefinitionExtension(reference))
    return {};

  std::filesystem::path path = PathUtils::PathFromUtf8(reference);
  if (path.is_relative())
    path = sceneBasePath / path;
  return PathUtils::PathToUtf8(path.lexically_normal());
}

// Finds the best reusable definition reference stored on a scene truss.
std::string GetTrussDefinitionPath(const Truss &truss,
                                   const std::filesystem::path &sceneBasePath) {
  for (const std::string *reference :
       {&truss.gdtfSpec, &truss.modelFile, &truss.symbolFile}) {
    std::string path = ResolveDefinitionPath(*reference, sceneBasePath);
    if (!path.empty())
      return path;
  }
  return {};
}

} // namespace

// Collects one reusable creation entry per available truss type.
std::vector<TrussCreationSource> CollectTrussCreationSources(
    const std::unordered_map<std::string, Truss> &trusses,
    const std::string &sceneBasePath) {
  const std::filesystem::path basePath = PathUtils::PathFromUtf8(sceneBasePath);
  std::map<std::string, std::string> sourceByType;
  for (const auto &[uuid, truss] : trusses) {
    const std::string displayName = GetTrussTypeDisplayName(truss);
    const std::string definitionPath = GetTrussDefinitionPath(truss, basePath);
    if (!displayName.empty() && !definitionPath.empty())
      sourceByType.try_emplace(displayName, definitionPath);
  }

  std::vector<TrussCreationSource> sources;
  sources.reserve(sourceByType.size());
  for (const auto &[displayName, definitionPath] : sourceByType)
    sources.push_back({displayName, definitionPath});
  return sources;
}

} // namespace gui
