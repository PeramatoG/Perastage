#include "mesh_processing.h"
#include "filesystem_path_utils.h"

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
constexpr uint32_t kCacheVersion = 3u;
constexpr float kOverdrawThreshold = 1.05f;

struct CacheHeader {
  uint32_t magic = 0;
  uint32_t version = 0;
  int64_t sourceTimestampNs = 0;
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

// Builds the disk cache path for a source asset.
std::string BuildCachePath(const std::string &sourcePath) {
  return sourcePath + ".cache";
}

// Builds a stable hash for mode-specific GDTF disk cache names.
uint64_t HashCacheToken(const std::string &value) {
  uint64_t hash = 1469598103934665603ull;
  for (unsigned char c : value) {
    hash ^= c;
    hash *= 1099511628211ull;
  }
  return hash;
}

// Builds the disk cache path for a GDTF mode selection.
std::string BuildGdtfCachePath(const std::string &sourcePath,
                               const std::string &modeName) {
  if (modeName.empty())
    return BuildCachePath(sourcePath);
  return sourcePath + "." + std::to_string(HashCacheToken(modeName)) + ".cache";
}

int64_t GetSourceTimestampNs(const std::string &sourcePath) {
  std::error_code ec;
  const fs::file_time_type writeTime = fs::last_write_time(PathUtils::PathFromUtf8(sourcePath), ec);
  if (ec)
    return -1;
  return std::chrono::duration_cast<std::chrono::nanoseconds>(writeTime.time_since_epoch())
      .count();
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

bool ValidateHeader(const CacheHeader &header, uint32_t expectedMagic,
                    const std::string &sourcePath) {
  if (header.magic != expectedMagic || header.version != kCacheVersion)
    return false;
  const int64_t sourceTimestampNs = GetSourceTimestampNs(sourcePath);
  return sourceTimestampNs >= 0 && sourceTimestampNs == header.sourceTimestampNs;
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
  OptimizeIndices(mesh);
}

void OptimizeGdtfObjectsForRuntime(std::vector<GdtfObject> &objects) {
  for (GdtfObject &obj : objects)
    OptimizeMeshForRuntime(obj.mesh);
}

bool TryLoadMeshCache(const std::string &sourcePath, Mesh &outMesh) {
  std::ifstream in(BuildCachePath(sourcePath), std::ios::binary);
  if (!in.is_open())
    return false;

  CacheHeader header;
  if (!ReadRaw(in, header) || !ValidateHeader(header, kMeshCacheMagic, sourcePath))
    return false;
  return ReadMesh(in, outMesh);
}

bool TrySaveMeshCache(const std::string &sourcePath, const Mesh &mesh) {
  const int64_t sourceTimestampNs = GetSourceTimestampNs(sourcePath);
  if (sourceTimestampNs < 0)
    return false;

  std::ofstream out(BuildCachePath(sourcePath), std::ios::binary | std::ios::trunc);
  if (!out.is_open())
    return false;

  CacheHeader header;
  header.magic = kMeshCacheMagic;
  header.version = kCacheVersion;
  header.sourceTimestampNs = sourceTimestampNs;
  return WriteRaw(out, header) && WriteMesh(out, mesh);
}

// Loads mode-specific GDTF objects from a valid disk cache.
bool TryLoadGdtfCache(const std::string &gdtfPath,
                      const std::string &modeName,
                      std::vector<GdtfObject> &outObjects) {
  std::ifstream in(BuildGdtfCachePath(gdtfPath, modeName), std::ios::binary);
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
  return true;
}

// Saves mode-specific GDTF objects to the disk cache.
bool TrySaveGdtfCache(const std::string &gdtfPath,
                      const std::string &modeName,
                      const std::vector<GdtfObject> &objects) {
  const int64_t sourceTimestampNs = GetSourceTimestampNs(gdtfPath);
  if (sourceTimestampNs < 0)
    return false;

  std::ofstream out(BuildGdtfCachePath(gdtfPath, modeName),
                    std::ios::binary | std::ios::trunc);
  if (!out.is_open())
    return false;

  CacheHeader header;
  header.magic = kGdtfCacheMagic;
  header.version = kCacheVersion;
  header.sourceTimestampNs = sourceTimestampNs;
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
