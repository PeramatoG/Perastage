#include "mesh_processing_cache.h"

#include "meshoptimizer/src/meshoptimizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>

namespace {

constexpr uint32_t kCacheMagic = 0x43545350; // PSTC
constexpr uint32_t kCacheVersion = 1;
constexpr float kTargetReductionRatio = 0.1f;
constexpr float kMaxSimplificationError = 0.01f;
constexpr float kOverdrawThreshold = 1.05f;

struct MeshCacheHeader {
  uint32_t magic = kCacheMagic;
  uint32_t version = kCacheVersion;
  uint64_t gdtfHash = 0;
  int64_t gdtfTimestamp = 0;
  uint64_t modelPathHash = 0;
  uint32_t vertexFloatCount = 0;
  uint32_t indexCount = 0;
  uint32_t texcoordFloatCount = 0;
  uint32_t materialFlags = 0;
};

uint64_t HashString(const std::string &value) {
  uint64_t hash = 14695981039346656037ull;
  constexpr uint64_t prime = 1099511628211ull;
  for (unsigned char c : value) {
    hash ^= static_cast<uint64_t>(c);
    hash *= prime;
  }
  return hash;
}

int64_t ToUnixNanos(std::filesystem::file_time_type time) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             time.time_since_epoch())
      .count();
}

std::filesystem::path BuildCachePath(const std::string &gdtfPath,
                                     const std::string &modelPath) {
  std::filesystem::path gdtf(gdtfPath);
  const std::string modelKey = std::to_string(HashString(modelPath));
  return gdtf.string() + ".cache." + modelKey + ".bin";
}

void RebuildNormalsIfNeeded(Mesh &mesh) {
  if (mesh.normals.size() == mesh.vertices.size())
    return;
  ComputeNormals(mesh);
}

bool WriteMeshCache(const std::filesystem::path &cachePath,
                    const MeshProcessingCacheContext &context,
                    const std::string &modelPath,
                    const Mesh &mesh) {
  std::ofstream stream(cachePath, std::ios::binary | std::ios::trunc);
  if (!stream.is_open())
    return false;

  MeshCacheHeader header;
  header.gdtfHash = context.gdtfContentHash;
  header.gdtfTimestamp = ToUnixNanos(context.gdtfTimestamp);
  header.modelPathHash = HashString(modelPath);
  header.vertexFloatCount = static_cast<uint32_t>(mesh.vertices.size());
  header.indexCount = static_cast<uint32_t>(mesh.indices.size());
  header.texcoordFloatCount = static_cast<uint32_t>(mesh.texcoords.size());
  if (mesh.hasMaterialBaseColor)
    header.materialFlags |= 0x1u;

  stream.write(reinterpret_cast<const char *>(&header), sizeof(header));
  stream.write(reinterpret_cast<const char *>(mesh.vertices.data()),
               static_cast<std::streamsize>(mesh.vertices.size() * sizeof(float)));
  stream.write(reinterpret_cast<const char *>(mesh.indices.data()),
               static_cast<std::streamsize>(mesh.indices.size() * sizeof(uint32_t)));
  stream.write(reinterpret_cast<const char *>(mesh.normals.data()),
               static_cast<std::streamsize>(mesh.normals.size() * sizeof(float)));
  stream.write(reinterpret_cast<const char *>(mesh.texcoords.data()),
               static_cast<std::streamsize>(mesh.texcoords.size() * sizeof(float)));
  stream.write(reinterpret_cast<const char *>(mesh.materialBaseColor.data()),
               static_cast<std::streamsize>(mesh.materialBaseColor.size() * sizeof(float)));
  return stream.good();
}

bool ReadMeshCache(const std::filesystem::path &cachePath,
                   const MeshProcessingCacheContext &context,
                   const std::string &modelPath,
                   Mesh &mesh) {
  std::ifstream stream(cachePath, std::ios::binary);
  if (!stream.is_open())
    return false;

  MeshCacheHeader header;
  stream.read(reinterpret_cast<char *>(&header), sizeof(header));
  if (!stream.good() || header.magic != kCacheMagic ||
      header.version != kCacheVersion)
    return false;

  if (header.gdtfHash != context.gdtfContentHash ||
      header.gdtfTimestamp != ToUnixNanos(context.gdtfTimestamp) ||
      header.modelPathHash != HashString(modelPath)) {
    return false;
  }

  mesh.vertices.resize(header.vertexFloatCount);
  mesh.indices.resize(header.indexCount);
  mesh.normals.resize(header.vertexFloatCount);
  mesh.texcoords.resize(header.texcoordFloatCount);

  stream.read(reinterpret_cast<char *>(mesh.vertices.data()),
              static_cast<std::streamsize>(mesh.vertices.size() * sizeof(float)));
  stream.read(reinterpret_cast<char *>(mesh.indices.data()),
              static_cast<std::streamsize>(mesh.indices.size() * sizeof(uint32_t)));
  stream.read(reinterpret_cast<char *>(mesh.normals.data()),
              static_cast<std::streamsize>(mesh.normals.size() * sizeof(float)));
  stream.read(reinterpret_cast<char *>(mesh.texcoords.data()),
              static_cast<std::streamsize>(mesh.texcoords.size() * sizeof(float)));
  stream.read(reinterpret_cast<char *>(mesh.materialBaseColor.data()),
              static_cast<std::streamsize>(mesh.materialBaseColor.size() * sizeof(float)));

  mesh.hasMaterialBaseColor = (header.materialFlags & 0x1u) != 0;
  mesh.flippedIndicesCache.clear();
  mesh.windingChecked = false;
  mesh.flatVertices.clear();
  mesh.flatNormals.clear();
  mesh.flippedFlatVertices.clear();
  mesh.flippedFlatNormals.clear();

  return stream.good();
}

} // namespace

void OptimizeMeshForGpu(Mesh &mesh) {
  if (mesh.indices.size() < 3 || mesh.vertices.size() < 9)
    return;

  const size_t vertexCount = mesh.vertices.size() / 3;
  const size_t targetIndexCount =
      std::max<size_t>(3, (mesh.indices.size() / 3) * 3 * kTargetReductionRatio);

  std::vector<uint32_t> simplified(mesh.indices.size());

  // Mesh decimation: aggressively reduce triangle count to ~10% while
  // respecting the configured geometric error cap.
  const size_t simplifiedCount = meshopt_simplify(
      simplified.data(), mesh.indices.data(), mesh.indices.size(),
      mesh.vertices.data(), vertexCount, sizeof(float) * 3, targetIndexCount,
      kMaxSimplificationError, 0, nullptr);

  if (simplifiedCount >= 3)
    simplified.resize(simplifiedCount);
  else
    simplified = mesh.indices;

  // Vertex cache optimization: reorder indices to improve post-transform cache
  // hit rates on the GPU.
  meshopt_optimizeVertexCache(simplified.data(), simplified.data(),
                              simplified.size(), vertexCount);

  // Overdraw optimization: reorder triangles preserving cache quality while
  // reducing fragment overdraw with a conservative threshold.
  meshopt_optimizeOverdraw(simplified.data(), simplified.data(), simplified.size(),
                           mesh.vertices.data(), vertexCount, sizeof(float) * 3,
                           kOverdrawThreshold);

  // Final vertex fetch optimization keeps vertex reads sequential in memory.
  std::vector<uint32_t> remap(vertexCount);
  const size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(
      remap.data(), simplified.data(), simplified.size(), vertexCount);

  std::vector<float> remappedVertices(uniqueVertices * 3);
  meshopt_remapVertexBuffer(remappedVertices.data(), mesh.vertices.data(),
                            vertexCount, sizeof(float) * 3, remap.data());
  meshopt_remapIndexBuffer(simplified.data(), simplified.data(), simplified.size(),
                           remap.data());

  mesh.vertices.swap(remappedVertices);
  mesh.indices.swap(simplified);

  if (!mesh.texcoords.empty()) {
    std::vector<float> remappedTexcoords(uniqueVertices * 2, 0.0f);
    meshopt_remapVertexBuffer(remappedTexcoords.data(), mesh.texcoords.data(),
                              vertexCount, sizeof(float) * 2, remap.data());
    mesh.texcoords.swap(remappedTexcoords);
  }

  mesh.normals.clear();
  RebuildNormalsIfNeeded(mesh);
}

bool LoadOrBuildOptimizedMesh(const std::string &modelPath,
                              const MeshProcessingCacheContext &context,
                              Mesh &mesh,
                              const std::function<bool(const std::string &, Mesh &)> &loader) {
  const std::filesystem::path cachePath = BuildCachePath(context.gdtfPath, modelPath);

  // Disk cache fast-path: skip parsing and optimization if a valid cache file
  // exists for the same GDTF content/timestamp.
  if (ReadMeshCache(cachePath, context, modelPath, mesh))
    return true;

  if (!loader(modelPath, mesh))
    return false;

  // First-load path: optimize geometry once and persist the processed payload.
  OptimizeMeshForGpu(mesh);
  RebuildNormalsIfNeeded(mesh);
  (void)WriteMeshCache(cachePath, context, modelPath, mesh);
  return true;
}
