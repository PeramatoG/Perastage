#include "truss_creation_source.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>

#include "filesystem_path_utils.h"

namespace gui {
namespace {

// Reports whether text is safe to show as a concise user-facing label.
bool IsUsableDisplayName(const std::string &value) {
  if (value.empty())
    return false;
  for (unsigned char ch : value) {
    if (std::iscntrl(ch) || ch == '/' || ch == '\\' || ch == '{' || ch == '}')
      return false;
  }
  return true;
}


// Reports whether a resource extension can define a reusable truss.
bool IsSupportedDefinitionExtension(const std::string &path) {
  std::string ext = PathUtils::PathFromUtf8(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return ext == ".gdtf" || ext == ".gtruss" || ext == ".glb" || ext == ".3ds";
}

// Resolves a supported truss definition reference against the scene directory.
std::string ResolveDefinitionPath(const std::string &reference,
                                  const std::filesystem::path &sceneBasePath) {
  if (reference.empty() || !IsSupportedDefinitionExtension(reference))
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

// Builds the readable type label shown when reusing a truss from the scene.
std::string GetTrussTypeDisplayName(const Truss &truss,
                                    const std::string &definitionPath) {
  if (!truss.gdtfSpec.empty() && IsUsableDisplayName(truss.model))
    return truss.model;
  if (truss.gdtfSpec.empty()) {
    if (IsUsableDisplayName(truss.name))
      return truss.name;
    const std::string stem =
        PathUtils::PathFromUtf8(definitionPath).stem().string();
    if (IsUsableDisplayName(stem))
      return stem;
  }
  if (IsUsableDisplayName(truss.model))
    return truss.model;
  if (IsUsableDisplayName(truss.name))
    return truss.name;
  return "Unnamed truss";
}

// Builds a stable source identity that is independent from display text.
std::string BuildSourceIdentityKey(const Truss &truss,
                                   const std::string &definitionPath) {
  if (!definitionPath.empty())
    return "path:" + definitionPath;
  if (!truss.perastageTypeKey.empty())
    return "type:" + truss.perastageTypeKey;
  return {};
}

} // namespace

// Collects one reusable creation entry per available truss definition.
std::vector<TrussCreationSource> CollectTrussCreationSources(
    const std::unordered_map<std::string, Truss> &trusses,
    const std::string &sceneBasePath) {
  const std::filesystem::path basePath = PathUtils::PathFromUtf8(sceneBasePath);
  std::map<std::string, TrussCreationSource> sourceByIdentity;
  for (const auto &[uuid, truss] : trusses) {
    const std::string definitionPath = GetTrussDefinitionPath(truss, basePath);
    const std::string identityKey = BuildSourceIdentityKey(truss, definitionPath);
    if (identityKey.empty() || definitionPath.empty())
      continue;
    const std::string displayName = GetTrussTypeDisplayName(truss, definitionPath);
    sourceByIdentity.try_emplace(identityKey,
                                 TrussCreationSource{identityKey, displayName,
                                                     definitionPath});
  }

  std::vector<TrussCreationSource> sources;
  sources.reserve(sourceByIdentity.size());
  for (const auto &[identityKey, source] : sourceByIdentity)
    sources.push_back(source);
  std::sort(sources.begin(), sources.end(), [](const auto &lhs, const auto &rhs) {
    if (lhs.displayName != rhs.displayName)
      return lhs.displayName < rhs.displayName;
    return lhs.identityKey < rhs.identityKey;
  });
  return sources;
}

} // namespace gui
