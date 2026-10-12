#include "apppaths.h"
#include "cache_service.h"
#include "resources/mesh_processing.h"
#include "filesystem_path_utils.h"
#include <cassert>
#include <chrono>
#include <fstream>

namespace fs = std::filesystem;
const auto testRoot = fs::temp_directory_path() /
    ("perastage-cache-service-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
namespace AppPaths {
fs::path GetUserDataDir() { return testRoot / "data"; }
}

void Write(const fs::path &path, const std::string &text) {
  fs::create_directories(path.parent_path());
  std::ofstream(path) << text;
}
void Legacy(const fs::path &path, bool gdtf) {
  uint32_t header[6] = {gdtf ? 0x48434747u : 0x4843534Du, 4, 0, 0, 0, 0};
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(header), sizeof(header));
}
int main() {
  using namespace viewer3d::resources;
  assert(AppPaths::GetCacheRoot() == AppPaths::GetUserDataDir() / "cache");
  assert(AppPaths::GetViewer3DGdtfCacheDir() == AppPaths::GetCacheRoot() / "viewer3d/gdtf");
  assert(AppPaths::GetViewer3DMeshCacheDir() == AppPaths::GetCacheRoot() / "viewer3d/meshes");
  assert(AppPaths::GetGdtfCatalogCachePath() == AppPaths::GetCacheRoot() / "gdtf/catalog.json");
  assert(CacheService::GetStatistics().Succeeded());
  assert(CacheService::GetStatistics().bytes == 0);
  const auto source = testRoot / "external" / "asset.glb";
  const auto gdtf = testRoot / "external" / "asset.gdtf";
  Write(source, "model"); Write(gdtf, "fixture");
  Mesh mesh; mesh.vertices = {1, 2, 3}; mesh.indices = {0};
  Mesh loaded;
  assert(TrySaveMeshCache(PathUtils::PathToUtf8(source), mesh));
  assert(!fs::exists(source.string() + ".cache"));
  assert(TryLoadMeshCache(PathUtils::PathToUtf8(source), loaded));
  assert(loaded.vertices == mesh.vertices && loaded.indices == mesh.indices);
  const auto path = CacheService::ViewerCachePath(PathUtils::PathToUtf8(source), "", false);
  assert(path == CacheService::ViewerCachePath(PathUtils::PathToUtf8(source.parent_path() / "./asset.glb"), "", false));
  assert(path != CacheService::ViewerCachePath(PathUtils::PathToUtf8(testRoot / "other/asset.glb"), "", false));
  std::vector<GdtfObject> objects(1); objects[0].mesh = mesh;
  std::vector<GdtfObject> result;
  for (const std::string mode : {"", "Mode A", "Mode B", "模式"}) {
    assert(TrySaveGdtfCache(PathUtils::PathToUtf8(gdtf), mode, objects));
    assert(TryLoadGdtfCache(PathUtils::PathToUtf8(gdtf), mode, result));
    assert(result.size() == 1 && result[0].mesh.vertices == mesh.vertices);
  }
  assert(!TryLoadGdtfCache(PathUtils::PathToUtf8(gdtf), "missing mode", result));
  assert(std::distance(fs::directory_iterator(source.parent_path()), fs::directory_iterator{}) == 2);
  const auto oldTime = fs::last_write_time(source);
  fs::last_write_time(source, oldTime + std::chrono::seconds(1));
  assert(!TryLoadMeshCache(PathUtils::PathToUtf8(source), loaded));
  assert(TrySaveMeshCache(PathUtils::PathToUtf8(source), mesh));
  Write(source, "changed size");
  assert(!TryLoadMeshCache(PathUtils::PathToUtf8(source), loaded));
  Write(gdtf, "changed fixture size");
  assert(!TryLoadGdtfCache(PathUtils::PathToUtf8(gdtf), "Mode A", result));
  assert(TrySaveMeshCache(PathUtils::PathToUtf8(source), mesh));
  // Existing format version validation remains mandatory.
  { std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    uint32_t invalid = 999; file.seekp(4); file.write(reinterpret_cast<char *>(&invalid), 4); }
  assert(!TryLoadMeshCache(PathUtils::PathToUtf8(source), loaded));
  const auto library = AppPaths::GetUserDataDir() / "library";
  Write(library / "fixtures/light.gdtf", "fixture");
  Legacy(library / "fixtures/light.gdtf.cache", true);
  Legacy(library / "fixtures/light.gdtf.12345.cache", true);
  Write(library / "scene_objects/object.3ds", "model");
  Legacy(library / "scene_objects/object.3ds.cache", false);
  Write(library / "fixtures/user.cache", "user data");
  Write(library / "fixtures/light.gdtf.999.cache", "unrelated cache");
  Legacy(library / "fixtures/missing.gdtf.cache", true);
  Legacy(source.string() + ".cache", false);
  std::error_code legacyLinkError;
  fs::create_directory_symlink(source.parent_path(), library / "fixtures/external", legacyLinkError);
  const auto cleanup = CacheService::CleanupLegacyViewerCaches(library);
  assert(cleanup.Succeeded() && cleanup.removedFiles == 3);
  assert(CacheService::CleanupLegacyViewerCaches(library).removedFiles == 0);
  assert(fs::exists(library / "fixtures/user.cache"));
  assert(fs::exists(library / "fixtures/light.gdtf.999.cache"));
  assert(fs::exists(library / "fixtures/missing.gdtf.cache"));
  assert(fs::exists(source.string() + ".cache"));
  assert(CacheService::Clear().Succeeded());
  Write(AppPaths::GetGdtfCatalogCachePath(), "12345");
  Write(AppPaths::GetViewer3DMeshCacheDir() / "nested" / (std::string(64, 'a') + ".cache"), "123");
  assert(CacheService::GetStatistics().bytes == 8);
  for (const auto *file : {"user_config.json", "perastage.log", "credentials", "dictionary.json", "project.mvr"})
    Write(AppPaths::GetUserDataDir() / file, "keep");
  const auto clear = CacheService::Clear();
  assert(clear.Succeeded() && clear.removedFiles == 2);
  assert(CacheService::GetStatistics().bytes == 0);
  assert(CacheService::Clear().Succeeded());
  assert(fs::is_directory(AppPaths::GetViewer3DGdtfCacheDir()));
  assert(fs::is_directory(AppPaths::GetViewer3DMeshCacheDir()));
  for (const auto *file : {"user_config.json", "perastage.log", "credentials", "dictionary.json", "project.mvr"})
    assert(fs::file_size(AppPaths::GetUserDataDir() / file) == 4);
  assert(fs::exists(library / "fixtures/light.gdtf"));
  assert(fs::exists(source.string() + ".cache"));
  assert(fs::exists(library / "fixtures/user.cache"));
  assert(!TryLoadMeshCache(PathUtils::PathToUtf8(source), loaded));
  assert(TrySaveMeshCache(PathUtils::PathToUtf8(source), mesh));
  assert(TryLoadMeshCache(PathUtils::PathToUtf8(source), loaded));
  // Even unexpected user resources inside the cache root are preserved.
  Write(AppPaths::GetCacheRoot() / "user.gdtf", "fixture");
  Write(AppPaths::GetCacheRoot() / "library/fixture.gdtf", "fixture");
  Write(AppPaths::GetViewer3DMeshCacheDir() / "project.mvr", "project");
  assert(!CacheService::Clear().Succeeded());
  assert(fs::file_size(AppPaths::GetCacheRoot() / "user.gdtf") == 7);
  assert(fs::file_size(AppPaths::GetViewer3DMeshCacheDir() / "project.mvr") == 7);
  assert(fs::file_size(AppPaths::GetCacheRoot() / "library/fixture.gdtf") == 7);
  fs::remove_all(AppPaths::GetCacheRoot() / "library");
  fs::remove(AppPaths::GetCacheRoot() / "user.gdtf");
  fs::remove(AppPaths::GetViewer3DMeshCacheDir() / "project.mvr");
  // Inaccessible contents report a partial failure while accessible files clear.
  const auto blocked = AppPaths::GetViewer3DMeshCacheDir() / "blocked";
  Write(blocked / (std::string(64, 'b') + ".cache"), "blocked");
  std::error_code permissionError;
  fs::permissions(blocked, fs::perms::none, permissionError);
  fs::directory_iterator probe(blocked, permissionError);
  if (permissionError) {
    assert(!CacheService::GetStatistics().Succeeded());
    const auto deletable = AppPaths::GetViewer3DMeshCacheDir() / (std::string(64, 'c') + ".cache");
    Write(deletable, "cache");
    const auto partial = CacheService::Clear();
    assert(!partial.Succeeded() && partial.removedFiles > 0);
    assert(!fs::exists(deletable));
  }
  fs::permissions(blocked, fs::perms::owner_all, permissionError);
  assert(CacheService::Clear().Succeeded());
  // Symlink escape attempts never traverse or delete user data.
  std::error_code ec;
  const auto link = AppPaths::GetCacheRoot() / "escape";
  fs::create_directory_symlink(source.parent_path(), link, ec);
  if (!ec) {
    // A malicious cache file cannot make a cache write overwrite source data.
    const auto meshCache = CacheService::ViewerCachePath(PathUtils::PathToUtf8(source), "", false);
    fs::create_symlink(source, meshCache, ec);
    assert(!ec && !TrySaveMeshCache(PathUtils::PathToUtf8(source), mesh));
    assert(fs::file_size(source) == std::string("changed size").size());
    fs::remove(meshCache);
    assert(!CacheService::GetStatistics().Succeeded());
    assert(!CacheService::Clear().Succeeded());
    assert(fs::exists(source) && fs::exists(gdtf));
    fs::remove(link);
    fs::remove_all(AppPaths::GetCacheRoot());
    fs::create_directory_symlink(source.parent_path(), AppPaths::GetCacheRoot());
    assert(!CacheService::Clear().Succeeded());
    assert(!TrySaveMeshCache(PathUtils::PathToUtf8(source), mesh));
    assert(fs::exists(source));
    fs::remove(AppPaths::GetCacheRoot());
  }
  fs::remove_all(testRoot);
}
