#include "resource_path_search.h"
#include "filesystem_path_utils.h"

#include "projectutils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace viewer3d::resources {
namespace {
using SearchClock = std::chrono::steady_clock;

bool MatchesFileNameWithExtensionTolerance(const fs::path &candidate,
                                           const std::string &fileName);

// Compares two ASCII strings without considering letter case.
bool EqualIgnoreCaseAscii(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    const unsigned char a = static_cast<unsigned char>(lhs[i]);
    const unsigned char b = static_cast<unsigned char>(rhs[i]);
    if (std::tolower(a) != std::tolower(b))
      return false;
  }
  return true;
}

// Checks whether a path is already present in a path list.
bool ContainsPath(const std::vector<fs::path> &paths, const fs::path &candidate) {
  return std::any_of(paths.begin(), paths.end(), [&](const fs::path &p) {
    return p == candidate;
  });
}

// Checks whether a candidate path matches a target file name with case-tolerant extensions.
bool MatchesFileNameWithExtensionTolerance(const fs::path &candidate,
                                           const std::string &fileName) {
  const fs::path target(fileName);
  const std::string candidateName = candidate.filename().string();
  if (candidateName == fileName)
    return true;

  if (candidate.stem().string() != target.stem().string())
    return false;
  return EqualIgnoreCaseAscii(candidate.extension().string(),
                              target.extension().string());
}

// Detects filesystem roots that should never be recursively scanned on the render path.
bool IsSkippedRecursiveSearchRoot(const fs::path &root, std::string *reason) {
  if (root.empty()) {
    if (reason)
      *reason = "empty search root";
    return true;
  }

  const std::string rootText = root.string();
  if (rootText.rfind("//", 0) == 0 || rootText.rfind("\\\\", 0) == 0) {
    if (reason)
      *reason = "network search root";
    return true;
  }

  const std::string rootName = root.filename().string();
  if (rootName == "Windows" || rootName == "Program Files" ||
      rootName == "Program Files (x86)" || rootName == "$Recycle.Bin" ||
      rootName == "System Volume Information" || rootName == "System") {
    if (reason)
      *reason = "system folder";
    return true;
  }

  std::error_code ec;
  const fs::path normalized = fs::weakly_canonical(root, ec);
  const fs::path probe = ec ? root.lexically_normal() : normalized;
#ifdef _WIN32
  const std::string probeText = probe.string();
  if (probeText.size() <= 3 && probe.has_root_directory()) {
    if (reason)
      *reason = "system drive root";
    return true;
  }
#else
  if (probe == fs::path("/")) {
    if (reason)
      *reason = "system filesystem root";
    return true;
  }
  static const std::array<fs::path, 4> systemRoots = {
      fs::path("/dev"), fs::path("/proc"), fs::path("/run"), fs::path("/sys")};
  for (const fs::path &systemRoot : systemRoots) {
    if (probe == systemRoot) {
      if (reason)
        *reason = "system folder";
      return true;
    }
  }
#endif

  return false;
}

// Checks whether a recursive search should avoid descending into a directory.
bool ShouldSkipRecursiveDirectory(const fs::path &path) {
  std::error_code ec;
  const fs::path canonical = fs::weakly_canonical(path, ec);
  const fs::path probe = ec ? path.lexically_normal() : canonical;
#ifndef _WIN32
  static const std::array<fs::path, 4> systemRoots = {
      fs::path("/dev"), fs::path("/proc"), fs::path("/run"), fs::path("/sys")};
  for (const fs::path &systemRoot : systemRoots) {
    if (probe == systemRoot)
      return true;
  }
#endif

  const std::string name = path.filename().string();
  return name == ".git" || name == ".svn" || name == ".hg" ||
         name == "node_modules" || name == "__pycache__" ||
         name == "Windows" || name == "Program Files" ||
         name == "Program Files (x86)" || name == "$Recycle.Bin" ||
         name == "System Volume Information" || name == "System";
}

// Searches recursively for a file while enforcing render-path safety limits.
BoundedRecursiveSearchResult FindFileRecursiveBounded(
    const std::string &baseDir, const std::string &fileName,
    const BoundedRecursiveSearchLimits &limits) {
  BoundedRecursiveSearchResult result;
  result.baseDir = baseDir;
  result.fileName = fileName;

  if (fileName.empty()) {
    result.skipped = true;
    result.skipReason = "empty file name";
    return result;
  }

  const fs::path root = PathUtils::PathFromUtf8(baseDir);
  if (IsSkippedRecursiveSearchRoot(root, &result.skipReason)) {
    result.skipped = true;
    return result;
  }

  const auto startTime = SearchClock::now();
  std::error_code ec;
  fs::recursive_directory_iterator it(
      root, fs::directory_options::skip_permission_denied, ec);
  if (ec) {
    result.skipped = true;
    result.skipReason = ec.message();
    return result;
  }

  const fs::recursive_directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }

    if (SearchClock::now() - startTime >= limits.maxElapsed) {
      result.capped = true;
      result.cappedByTime = true;
      break;
    }

    if (it->is_directory(ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      ++result.visitedDirectories;
      if (ShouldSkipRecursiveDirectory(it->path())) {
        it.disable_recursion_pending();
        result.skippedFolder = true;
        continue;
      }
      if (result.visitedDirectories >= limits.maxVisitedDirectories) {
        result.capped = true;
        result.cappedByDirectories = true;
        break;
      }
      continue;
    }

    if (!it->is_regular_file(ec) || ec) {
      ec.clear();
      continue;
    }

    ++result.visitedFiles;
    if (MatchesFileNameWithExtensionTolerance(it->path(), fileName)) {
      result.resolvedPath = it->path().string();
      return result;
    }

    if (result.visitedFiles >= limits.maxVisitedFiles) {
      result.capped = true;
      result.cappedByFiles = true;
      break;
    }
  }

  return result;
}

// Builds the bounded recursive fallback search roots in priority order.
std::vector<fs::path> BuildRecursiveFallbackRoots(const std::string &base) {
  std::vector<fs::path> roots;
  if (!base.empty())
    roots.push_back(PathUtils::PathFromUtf8(base));

  const std::array<std::string, 3> librarySubdirs = {
      "fixtures", "trusses", "scene_objects"};
  for (const std::string &subdir : librarySubdirs) {
    const fs::path root = PathUtils::PathFromUtf8(ProjectUtils::GetDefaultLibraryPath(subdir));
    if (!root.empty() && !ContainsPath(roots, root))
      roots.push_back(root);
  }
  return roots;
}

// Records recursive-search diagnostics that should be logged by the caller.
void AddRecursiveSearchDiagnostic(
    BoundedRecursiveSearchDiagnostics *diagnostics,
    const BoundedRecursiveSearchResult &result) {
  if (!diagnostics || (!result.skipped && !result.capped && !result.skippedFolder))
    return;
  diagnostics->push_back(result);
}

} // namespace

// Searches fallback roots recursively while preserving configured traversal bounds.
std::string ResolveFromBoundedRecursiveFallback(
    const std::string &base, const std::string &fileName,
    BoundedRecursiveSearchDiagnostics *diagnostics) {
  const BoundedRecursiveSearchLimits limits;
  for (const fs::path &root : BuildRecursiveFallbackRoots(base)) {
    BoundedRecursiveSearchResult result =
        FindFileRecursiveBounded(root.string(), fileName, limits);
    const std::string resolvedPath = result.resolvedPath;
    AddRecursiveSearchDiagnostic(diagnostics, result);
    if (!resolvedPath.empty())
      return resolvedPath;
  }
  return {};
}

} // namespace viewer3d::resources
