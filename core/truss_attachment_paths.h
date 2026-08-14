#pragma once

#include "mvrscene.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace truss_attachment_paths {

enum class Provenance {
  ExplicitStandardStructure,
  GdtfStructureGeometry,
  GdtfModelGeometry,
  MvrGeometry,
  ApproximateBoundsFallback
};

struct Diagnostics {
  float confidence = 0.0f;
  float longitudinalCoverage = 0.0f;
  float transverseStability = 0.0f;
  int occupiedSamples = 0;
  int totalSamples = 0;
};

// Represents a runtime-only, curve-ready fixture mounting path.
struct Path {
  std::string stableId;
  std::string ownerTrussUuid;
  std::vector<std::array<float, 3>> localPointsMm;
  std::vector<std::array<float, 3>> worldPointsMm;
  std::optional<float> estimatedRadiusMm;
  std::optional<std::array<float, 3>> localOutwardDirection;
  std::optional<std::array<float, 3>> worldOutwardDirection;
  Provenance provenance = Provenance::MvrGeometry;
  Diagnostics diagnostics;
};

struct ClosestPoint {
  std::array<float, 3> pointMm{};
  float distanceMm = 0.0f;
  float pathParameter = 0.0f;
};

struct Resolution {
  std::vector<Path> paths;
  std::vector<std::string> diagnostics;
  std::string sourceIdentity;
  bool usedBoundsFallback = false;
};

// Resolves and caches immutable local fixture attachment geometry.
class Resolver {
public:
  struct GdtfSource {
    std::string modelFile;
    Matrix position{};
    Provenance provenance = Provenance::GdtfModelGeometry;
    std::string role;
  };

  Resolution Resolve(const MvrScene &scene, const Truss &truss);
  void Clear();
  std::size_t GeometryParseCount() const;
  std::size_t ArchiveParseCount() const;

private:
  struct CacheEntry {
    std::vector<Path> localPaths;
    std::vector<std::string> diagnostics;
    std::string sourceIdentity;
  };
  struct ArchiveCacheEntry {
    std::vector<GdtfSource> sources;
    std::string sourceIdentity;
  };
  std::map<std::string, CacheEntry> m_cache;
  std::map<std::string, ArchiveCacheEntry> m_archiveCache;
  std::size_t m_geometryParseCount = 0;
  std::size_t m_archiveParseCount = 0;
};

// Detects persistent longitudinal chord paths in one indexed local mesh.
std::vector<Path> AnalyzeMesh(const std::vector<float> &verticesMm,
                              const std::vector<std::uint32_t> &triangleIndices,
                              Provenance provenance = Provenance::MvrGeometry);

// Returns the closest point on any segment of a path polyline.
std::optional<ClosestPoint>
ClosestPointOnPath(const Path &path, const std::array<float, 3> &pointMm,
                   bool worldSpace = true);

// Applies an instance transform to a local path without changing cached data.
Path TransformPath(const Path &localPath, const Matrix &transform,
                   const std::string &ownerTrussUuid);

} // namespace truss_attachment_paths
