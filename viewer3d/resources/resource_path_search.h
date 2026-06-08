#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace viewer3d::resources {

struct BoundedRecursiveSearchLimits {
  size_t maxVisitedFiles = 1500;
  size_t maxVisitedDirectories = 250;
  std::chrono::milliseconds maxElapsed = std::chrono::milliseconds(25);
};

struct BoundedRecursiveSearchResult {
  std::string resolvedPath;
  std::string baseDir;
  std::string fileName;
  std::string skipReason;
  size_t visitedFiles = 0;
  size_t visitedDirectories = 0;
  bool skipped = false;
  bool capped = false;
  bool cappedByFiles = false;
  bool cappedByDirectories = false;
  bool cappedByTime = false;
  bool skippedFolder = false;
};

using BoundedRecursiveSearchDiagnostics =
    std::vector<BoundedRecursiveSearchResult>;

std::string ResolveFromBoundedRecursiveFallback(
    const std::string &base, const std::string &fileName,
    BoundedRecursiveSearchDiagnostics *diagnostics = nullptr);

} // namespace viewer3d::resources
