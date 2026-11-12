#include "mvr_model_resource_closure.h"
#include "mvr_model_dependencies.h"
#include "filesystem_path_utils.h"
#include "file_import_utils.h"
#include "uuidutils.h"
#include <algorithm>
#include <set>

namespace mvr {
namespace {
namespace fs = std::filesystem;
bool Within(const fs::path &path, const fs::path &root) {
  const auto relative = path.lexically_relative(root);
  return !relative.empty() && !relative.is_absolute() &&
         *relative.begin() != "..";
}
std::string Lower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}
bool DecodeUri(std::string &value) {
  std::string decoded;
  const auto hex = [](char c) {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  };
  for (size_t index = 0; index < value.size(); ++index) {
    if (value[index] != '%')
      decoded += value[index];
    else {
      if (index + 2 >= value.size() || hex(value[index + 1]) < 0 ||
          hex(value[index + 2]) < 0)
        return false;
      decoded += char(hex(value[index + 1]) * 16 + hex(value[index + 2]));
      index += 2;
    }
  }
  value = std::move(decoded);
  return true;
}
} // namespace
std::string ModelResourceRelativeName(const fs::path &file,
                                      const fs::path &root) {
  const auto value = file.lexically_relative(root).generic_u8string();
  return {value.begin(), value.end()};
}
std::string
ModelResourcePrefix(const ModelResourceClosure &closure,
                    const std::function<bool(const std::string &)> &available) {
  std::string seed = "MVR model resource tree";
  for (const auto &file : closure.files) {
    const auto hash = FileImportUtils::ComputeFileSha256(file);
    if (!hash)
      return {};
    seed += ModelResourceRelativeName(file, closure.root) + ":" + *hash + ";";
  }
  const auto base = "model-" + DeriveDeterministicUuid(seed);
  for (size_t suffix = 0;; ++suffix) {
    const auto candidate =
        base + (suffix ? "-" + std::to_string(suffix) : "") + "/";
    if (available(candidate))
      return candidate;
  }
}
ModelResourceClosure DiscoverModelResourceClosure(const fs::path &model,
                                                  const fs::path &packageRoot) {
  ModelResourceClosure result;
  std::error_code error;
  auto primary = fs::weakly_canonical(model, error);
  if (error || !fs::is_regular_file(primary, error)) {
    result.missingResource = true;
    result.error =
        "Model resource is unavailable: " + PathUtils::PathToUtf8(model);
    return result;
  }
  auto boundary = fs::weakly_canonical(packageRoot, error);
  const bool constrained =
      !packageRoot.empty() && !error && Within(primary, boundary);
  result.root = primary.parent_path();
  std::set<fs::path> seen;
  std::vector<fs::path> pending{primary};
  while (!pending.empty()) {
    auto file = pending.back();
    pending.pop_back();
    if (!seen.insert(file).second)
      continue;
    result.files.push_back(file);
    const auto extension = Lower(file.extension().string());
    if (extension != ".3ds" && extension != ".gltf" && extension != ".glb")
      continue;
    auto inspected = InspectModelDependencies(file);
    if (!inspected.error.empty()) {
      result.error =
          inspected.error + " Resource: " + PathUtils::PathToUtf8(file);
      return result;
    }
    for (auto reference : inspected.references) {
      // glTF uses URI escaping; 3DS texture references are literal filenames.
      if (extension != ".3ds" && !DecodeUri(reference)) {
        result.error = "Model dependency has malformed URI escaping.";
        return result;
      }
      std::replace(reference.begin(), reference.end(), '\\', '/');
      const auto relative = PathUtils::PathFromUtf8(reference);
      if (relative.is_absolute() || reference.find(':') != std::string::npos ||
          reference.find('\0') != std::string::npos) {
        result.error =
            "Model dependency must be a portable relative URI: " + reference;
        return result;
      }
      auto target = fs::weakly_canonical(file.parent_path() / relative, error);
      if (error || (constrained && !Within(target, boundary))) {
        result.error =
            "Model dependency leaves its acquired package: " + reference;
        return result;
      }
      if (!fs::is_regular_file(target, error)) {
        // ZIP spelling comparisons are case-insensitive for legacy archives.
        const auto parent = target.parent_path();
        for (const auto &entry : fs::directory_iterator(
                 parent, fs::directory_options::skip_permission_denied,
                 error)) {
          if (error)
            break;
          if (entry.is_regular_file() &&
              Lower(entry.path().filename().string()) ==
                  Lower(target.filename().string())) {
            target = entry.path();
            break;
          }
        }
        if (!fs::is_regular_file(target, error)) {
          result.missingResource = true;
          result.error =
              "Required model dependency is unavailable: " + reference;
          return result;
        }
      }
      target = fs::weakly_canonical(target, error);
      if (error || (constrained && !Within(target, boundary))) {
        result.error =
            "Model dependency leaves its acquired package: " + reference;
        return result;
      }
      while (!Within(target, result.root) && result.root.has_parent_path() &&
             result.root != result.root.parent_path())
        result.root = result.root.parent_path();
      result.dependencies[file].push_back(target);
      pending.push_back(target);
    }
  }
  std::sort(result.files.begin(), result.files.end());
  return result;
}
} // namespace mvr
