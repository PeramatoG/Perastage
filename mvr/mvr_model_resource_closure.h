#pragma once
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace mvr {
struct ModelResourceClosure {
  std::filesystem::path root;
  std::vector<std::filesystem::path> files;
  std::map<std::filesystem::path, std::vector<std::filesystem::path>>
      dependencies;
  std::string error;
  bool missingResource = false;
};
// Discover the original resource tree without rewriting model bytes or URIs.
// Relative '..' references must remain inside the acquired package root.
std::string ModelResourceRelativeName(const std::filesystem::path &file,
                                      const std::filesystem::path &root);
std::string
ModelResourcePrefix(const ModelResourceClosure &closure,
                    const std::function<bool(const std::string &)> &available);
ModelResourceClosure
DiscoverModelResourceClosure(const std::filesystem::path &model,
                             const std::filesystem::path &packageRoot = {});
} // namespace mvr
