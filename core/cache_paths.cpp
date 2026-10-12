#include "apppaths.h"

namespace AppPaths {
std::filesystem::path GetCacheRoot() { return GetUserDataDir() / "cache"; }
std::filesystem::path GetViewer3DGdtfCacheDir() {
  return GetCacheRoot() / "viewer3d" / "gdtf";
}
std::filesystem::path GetViewer3DMeshCacheDir() {
  return GetCacheRoot() / "viewer3d" / "meshes";
}
std::filesystem::path GetGdtfCatalogCachePath() {
  return GetCacheRoot() / "gdtf" / "catalog.json";
}
// Migration state survives Clear Cache so old copies cannot resurrect a cache.
std::filesystem::path GetCatalogCacheMigrationMarker() {
  return GetUserDataDir() / "cache_migrations" / "gdtf_catalog";
}
} // namespace AppPaths
