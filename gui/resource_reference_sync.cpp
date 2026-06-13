#include "resource_reference_sync.h"
#include "filesystem_path_utils.h"

#include <cctype>
#include <filesystem>
#include <system_error>

namespace gui {
namespace {

namespace fs = std::filesystem;

// Trims table path text copied from wx controls before comparing references.
std::string TrimPathRef(std::string value) {
  auto isTrim = [](unsigned char ch) {
    return std::isspace(ch) != 0 || ch == '"' || ch == '\'';
  };
  while (!value.empty() && isTrim(static_cast<unsigned char>(value.front())))
    value.erase(value.begin());
  while (!value.empty() && isTrim(static_cast<unsigned char>(value.back())))
    value.pop_back();
  return value;
}

// Detects virtual primitive references that should not be converted into paths.
bool IsPrimitiveReference(const std::string &value) {
  return value.rfind("primitive:", 0) == 0;
}

// Resolves scene-relative resource references against the current MVR base path.
fs::path ResolveSceneResourcePath(const std::string &basePath,
                                  const std::string &pathRef) {
  fs::path path = PathUtils::PathFromUtf8(pathRef);
  if (!path.is_absolute() && !basePath.empty())
    path = PathUtils::PathFromUtf8(basePath) / path;
  return path.lexically_normal();
}

// Builds a stable comparable path even when the target is temporarily missing.
fs::path ComparableResourcePath(const fs::path &path) {
  std::error_code ec;
  fs::path canonical = fs::weakly_canonical(path, ec);
  if (!ec)
    return canonical.lexically_normal();
  fs::path absolute = fs::absolute(path, ec);
  return (ec ? path : absolute).lexically_normal();
}

// Returns whether two scene resource references point at the same filesystem path.
bool ReferencesSameResource(const std::string &basePath,
                            const std::string &leftRef,
                            const std::string &rightRef) {
  const std::string left = TrimPathRef(leftRef);
  const std::string right = TrimPathRef(rightRef);
  if (left.empty() || right.empty())
    return false;
  if (IsPrimitiveReference(left) || IsPrimitiveReference(right))
    return left == right;
  const fs::path leftPath = ResolveSceneResourcePath(basePath, left);
  const fs::path rightPath = ResolveSceneResourcePath(basePath, right);
  std::error_code ec;
  if (fs::equivalent(leftPath, rightPath, ec))
    return true;
  return ComparableResourcePath(leftPath) == ComparableResourcePath(rightPath);
}

} // namespace

// Preserves existing scene references when table caches still point to the same resource.
std::string PreserveSceneResourceReferenceForTableSync(
    const std::string &basePath, const std::string &currentRef,
    const std::string &candidateRef, const std::string &displayOnlyRef) {
  const std::string candidate = TrimPathRef(candidateRef);
  if (candidate.empty())
    return candidate;

  const std::string current = TrimPathRef(currentRef);
  if (!current.empty() && ReferencesSameResource(basePath, current, candidate))
    return currentRef;

  if (current.empty() && !displayOnlyRef.empty() &&
      ReferencesSameResource(basePath, displayOnlyRef, candidate))
    return currentRef;

  return candidate;
}

} // namespace gui
