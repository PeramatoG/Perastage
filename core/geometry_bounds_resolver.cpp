#include "geometry_bounds_resolver.h"

#include "filesystem_path_utils.h"
#include "loader3ds.h"
#include "loaderglb.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>

namespace {
std::mutex g_cacheMutex;
std::map<std::string, std::optional<GeometryBounds>> g_cache;
std::size_t g_parseCount = 0;

// Builds a resource-version key that changes when the underlying file changes.
std::string BuildVersionKey(const std::filesystem::path &path) {
  std::error_code ec;
  const auto absolute = std::filesystem::weakly_canonical(path, ec);
  if (ec)
    return {};
  const auto size = std::filesystem::file_size(absolute, ec);
  if (ec)
    return {};
  const auto timestamp = std::filesystem::last_write_time(absolute, ec);
  if (ec)
    return {};
  return PathUtils::BuildFilesystemIdentityKey(absolute) + "#" +
         std::to_string(size) + "#" +
         std::to_string(timestamp.time_since_epoch().count());
}

// Measures finite vertex positions already converted to renderer millimeters.
std::optional<GeometryBounds> MeasureMesh(const Mesh &mesh) {
  if (mesh.vertices.size() < 3 || mesh.vertices.size() % 3 != 0)
    return std::nullopt;
  GeometryBounds bounds{{mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]},
                        {mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]}};
  for (std::size_t offset = 0; offset < mesh.vertices.size(); offset += 3) {
    for (int axis = 0; axis < 3; ++axis) {
      const float value = mesh.vertices[offset + axis];
      if (!std::isfinite(value))
        return std::nullopt;
      bounds.minMm[axis] = std::min(bounds.minMm[axis], value);
      bounds.maxMm[axis] = std::max(bounds.maxMm[axis], value);
    }
  }
  return bounds.IsValid() ? std::optional<GeometryBounds>(bounds) : std::nullopt;
}
} // namespace

// Resolves bounds for a supported 3DS or GLB resource.
std::optional<GeometryBounds>
GeometryBoundsResolver::Resolve(const std::filesystem::path &path,
                                std::string *diagnostic) {
  const std::string key = BuildVersionKey(path);
  if (key.empty()) {
    if (diagnostic)
      *diagnostic = "Geometry file is missing or unreadable";
    return std::nullopt;
  }
  std::lock_guard<std::mutex> lock(g_cacheMutex);
  if (const auto found = g_cache.find(key); found != g_cache.end())
    return found->second;

  Mesh mesh;
  std::string error;
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  ++g_parseCount;
  const bool loaded = ext == ".3ds" ? Load3DS(path.string(), mesh, true, &error)
                                     : ext == ".glb" ? LoadGLB(path.string(), mesh, &error)
                                                     : false;
  auto bounds = loaded ? MeasureMesh(mesh) : std::nullopt;
  g_cache.emplace(key, bounds);
  if (!bounds && diagnostic)
    *diagnostic = error.empty() ? "Geometry has no usable three-dimensional bounds" : error;
  return bounds;
}

// Returns the number of physical parses, for cache regression tests.
std::size_t GeometryBoundsResolver::ParseCountForTesting() { return g_parseCount; }

// Clears process-local cached measurements, for deterministic tests.
void GeometryBoundsResolver::ClearForTesting() {
  std::lock_guard<std::mutex> lock(g_cacheMutex);
  g_cache.clear();
  g_parseCount = 0;
}
