#include "performance_metrics.h"
#include "mesh_processing.h"
#include "cache_service.h"
#include "filesystem_path_utils.h"
#include "physical_asset_revision.h"
#include "packaged_resource_identity.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include <meshoptimizer.h>

namespace fs = std::filesystem;

namespace viewer3d::resources {
namespace {

constexpr uint32_t kMeshCacheMagic = 0x4843534Du; // MSCH
constexpr uint32_t kGdtfCacheMagic = 0x48434747u; // GGCH
constexpr uint32_t kCacheVersion = 4u;
constexpr float kOverdrawThreshold = 1.05f;

struct CacheLookupMetric {
  bool hit = false;
  CacheLookupMetric() { performance::Count(performance::Work::CacheLookup); }
  ~CacheLookupMetric() { performance::Count(hit ? performance::Work::CacheHit : performance::Work::CacheMiss); }
};

struct CacheHeader {
  uint32_t magic = 0;
  uint32_t version = 0;
  int64_t sourceTimestampNs = 0;
  uint64_t sourceSize = 0;
};

template <typename T>
bool WriteRaw(std::ofstream &out, const T &value) {
  out.write(reinterpret_cast<const char *>(&value), sizeof(T));
  return out.good();
}

template <typename T>
bool ReadRaw(std::ifstream &in, T &value) {
  in.read(reinterpret_cast<char *>(&value), sizeof(T));
  return in.good();
}

template <typename T>
bool WriteVector(std::ofstream &out, const std::vector<T> &values) {
  const uint64_t count = static_cast<uint64_t>(values.size());
  if (!WriteRaw(out, count))
    return false;
  if (count == 0)
    return true;
  out.write(reinterpret_cast<const char *>(values.data()),
            static_cast<std::streamsize>(sizeof(T) * values.size()));
  return out.good();
}

template <typename T>
bool ReadVector(std::ifstream &in, std::vector<T> &values) {
  uint64_t count = 0;
  if (!ReadRaw(in, count))
    return false;
  if (count > (std::numeric_limits<size_t>::max() / sizeof(T)))
    return false;
  values.resize(static_cast<size_t>(count));
  if (count == 0)
    return true;
  in.read(reinterpret_cast<char *>(values.data()),
          static_cast<std::streamsize>(sizeof(T) * values.size()));
  return in.good();
}

bool WriteMesh(std::ofstream &out, const Mesh &mesh) {
  if (!WriteVector(out, mesh.vertices) || !WriteVector(out, mesh.indices) ||
      !WriteVector(out, mesh.normals) || !WriteVector(out, mesh.texcoords) ||
      !WriteVector(out, mesh.textureRgba)) {
    return false;
  }

  if (!WriteRaw(out, mesh.textureWidth) || !WriteRaw(out, mesh.textureHeight) ||
      !WriteRaw(out, mesh.materialBaseColor) ||
      !WriteRaw(out, mesh.hasMaterialBaseColor)) {
    return false;
  }
  return true;
}

bool ReadMesh(std::ifstream &in, Mesh &mesh) {
  Mesh loaded;
  if (!ReadVector(in, loaded.vertices) || !ReadVector(in, loaded.indices) ||
      !ReadVector(in, loaded.normals) || !ReadVector(in, loaded.texcoords) ||
      !ReadVector(in, loaded.textureRgba)) {
    return false;
  }

  if (!ReadRaw(in, loaded.textureWidth) || !ReadRaw(in, loaded.textureHeight) ||
      !ReadRaw(in, loaded.materialBaseColor) ||
      !ReadRaw(in, loaded.hasMaterialBaseColor)) {
    return false;
  }

  mesh = std::move(loaded);
  return true;
}

// Validates that a disk cache belongs to the current physical source revision.
bool ValidateHeader(const CacheHeader &header, uint32_t expectedMagic,
                    const std::string &sourcePath) {
  if (header.magic != expectedMagic || header.version != kCacheVersion)
    return false;
  const PhysicalAssetRevision revision = ReadPhysicalAssetRevision(sourcePath, false);
  return revision.metadataAvailable &&
         (packaged_resources::Lookup(PathUtils::PathFromUtf8(sourcePath))
              ? header.sourceTimestampNs == 0
              : revision.modificationTimeNs == header.sourceTimestampNs) &&
         revision.fileSize == header.sourceSize;
}

void OptimizeIndices(Mesh &mesh) {
  if (mesh.vertices.size() < 9 || mesh.indices.size() < 3)
    return;

  const size_t vertexCount = mesh.vertices.size() / 3;
  std::vector<uint32_t> cacheOptimized(mesh.indices.size());
  // Runtime/base meshes keep full geometry and only reorder triangles to
  // improve GPU locality without changing visual fidelity.
  meshopt_optimizeVertexCache(cacheOptimized.data(), mesh.indices.data(),
                              mesh.indices.size(), vertexCount);

  std::vector<uint32_t> overdrawOptimized(cacheOptimized.size());
  // Overdraw optimization with conservative threshold.
  meshopt_optimizeOverdraw(overdrawOptimized.data(), cacheOptimized.data(),
                           cacheOptimized.size(), mesh.vertices.data(),
                           vertexCount, sizeof(float) * 3, kOverdrawThreshold);

  mesh.indices = std::move(overdrawOptimized);
}

} // namespace

void OptimizeMeshForRuntime(Mesh &mesh) {
  performance::Scope metric(performance::Work::MeshProcessing);
  OptimizeIndices(mesh);
}

void OptimizeGdtfObjectsForRuntime(std::vector<GdtfObject> &objects) {
  for (GdtfObject &obj : objects)
    OptimizeMeshForRuntime(obj.mesh);
}

bool TryLoadMeshCache(const std::string &sourcePath, Mesh &outMesh) {
  performance::Scope metric(performance::Work::CacheRead);
  CacheLookupMetric lookup;
  const auto path = CacheService::ViewerCachePath(sourcePath, "", false);
  if (!CacheService::IsSafeCacheFile(path)) return false;
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open())
    return false;

  CacheHeader header;
  if (!ReadRaw(in, header) || !ValidateHeader(header, kMeshCacheMagic, sourcePath))
    return false;
  const bool loaded = ReadMesh(in, outMesh);
  lookup.hit = loaded;
  return loaded;
}

// Saves a model mesh with the bounded source revision used for validation.
bool TrySaveMeshCache(const std::string &sourcePath, const Mesh &mesh) {
  performance::Scope metric(performance::Work::CacheWrite);
  const PhysicalAssetRevision revision = ReadPhysicalAssetRevision(sourcePath, false);
  if (!revision.metadataAvailable || !CacheService::EnsureDirectories())
    return false;

  const auto path = CacheService::ViewerCachePath(sourcePath, "", false);
  if (!CacheService::IsSafeCacheFile(path)) return false;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open())
    return false;

  CacheHeader header;
  header.magic = kMeshCacheMagic;
  header.version = kCacheVersion;
  header.sourceTimestampNs = packaged_resources::Lookup(PathUtils::PathFromUtf8(sourcePath))
      ? 0 : revision.modificationTimeNs;
  header.sourceSize = revision.fileSize;
  return WriteRaw(out, header) && WriteMesh(out, mesh);
}

// Loads mode-specific GDTF objects from a valid disk cache.
bool TryLoadGdtfCache(const std::string &gdtfPath,
                      const std::string &modeName,
                      std::vector<GdtfObject> &outObjects) {
  performance::Scope metric(performance::Work::CacheRead);
  CacheLookupMetric lookup;
  const auto path = CacheService::ViewerCachePath(gdtfPath, modeName, true);
  if (!CacheService::IsSafeCacheFile(path)) return false;
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open())
    return false;

  CacheHeader header;
  if (!ReadRaw(in, header) || !ValidateHeader(header, kGdtfCacheMagic, gdtfPath))
    return false;

  uint64_t count = 0;
  if (!ReadRaw(in, count))
    return false;

  std::vector<GdtfObject> loaded;
  loaded.resize(static_cast<size_t>(count));
  for (GdtfObject &object : loaded) {
    if (!ReadRaw(in, object.transform) || !ReadRaw(in, object.isLens) ||
        !ReadMesh(in, object.mesh)) {
      return false;
    }
  }

  outObjects = std::move(loaded);
  lookup.hit = true;
  return true;
}

// Saves mode-specific GDTF objects to the disk cache.
bool TrySaveGdtfCache(const std::string &gdtfPath,
                      const std::string &modeName,
                      const std::vector<GdtfObject> &objects) {
  performance::Scope metric(performance::Work::CacheWrite);
  const PhysicalAssetRevision revision = ReadPhysicalAssetRevision(gdtfPath, false);
  if (!revision.metadataAvailable || !CacheService::EnsureDirectories())
    return false;

  const auto path = CacheService::ViewerCachePath(gdtfPath, modeName, true);
  if (!CacheService::IsSafeCacheFile(path)) return false;
  std::ofstream out(path,
                    std::ios::binary | std::ios::trunc);
  if (!out.is_open())
    return false;

  CacheHeader header;
  header.magic = kGdtfCacheMagic;
  header.version = kCacheVersion;
  header.sourceTimestampNs = packaged_resources::Lookup(PathUtils::PathFromUtf8(gdtfPath))
      ? 0 : revision.modificationTimeNs;
  header.sourceSize = revision.fileSize;
  if (!WriteRaw(out, header))
    return false;

  if (!WriteRaw(out, static_cast<uint64_t>(objects.size())))
    return false;

  for (const GdtfObject &object : objects) {
    if (!WriteRaw(out, object.transform) || !WriteRaw(out, object.isLens) ||
        !WriteMesh(out, object.mesh)) {
      return false;
    }
  }

  return true;
}

} // namespace viewer3d::resources
