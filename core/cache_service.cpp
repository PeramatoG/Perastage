#include "cache_service.h"
#include "apppaths.h"
#include "file_import_utils.h"
#include "filesystem_path_utils.h"
#include "packaged_resource_identity.h"
#include "performance_metrics.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace fs = std::filesystem;
namespace CacheService {
namespace {
thread_local ViewerCacheBatch *activeBatch = nullptr;

// Recheck the owned tree without repeating canonicalization/directory creation.
bool IsSafePreparedParent(const fs::path &directory) {
  if (!activeBatch || !activeBatch->prepared ||
      std::find(activeBatch->directories.begin(), activeBatch->directories.end(),
                directory) == activeBatch->directories.end())
    return IsSafeDirectory(directory);
  const auto data = AppPaths::GetUserDataDir();
  for (auto current = directory; ; current = current.parent_path()) {
    std::error_code ec;
    const auto status = fs::symlink_status(current, ec);
    if (ec || !fs::is_directory(status) || fs::is_symlink(status)) return false;
    if (current == data) return true;
    if (current.empty() || current == current.parent_path()) return false;
  }
}
void Error(Result &result, const fs::path &path, const std::string &message) {
  result.errors.push_back(PathUtils::PathToUtf8(path) + ": " + message);
}

bool InViewerBucket(const fs::path &relative) {
  for (const auto &bucket : {fs::path("viewer3d") / "gdtf",
                             fs::path("viewer3d") / "meshes"}) {
    const auto tail = relative.lexically_relative(bucket);
    if (!tail.empty() && *tail.begin() != "..") return true;
  }
  return false;
}

bool IsOwnedCacheDirectory(const fs::path &path) {
  const auto relative = path.lexically_relative(AppPaths::GetCacheRoot());
  return relative == "viewer3d" || relative == "gdtf" || InViewerBucket(relative);
}

bool IsOwnedCacheFile(const fs::path &path) {
  const auto relative = path.lexically_relative(AppPaths::GetCacheRoot());
  const auto name = path.filename().string();
  const auto catalog = AppPaths::GetGdtfCatalogCachePath().filename().string();
  if (relative.parent_path() == "gdtf")
    return name == catalog || name.starts_with(catalog + ".tmp.") ||
           name.starts_with(catalog + ".backup.");
  if (!InViewerBucket(relative.parent_path())) return false;
  return name.size() == 70 && name.ends_with(".cache") &&
         std::all_of(name.begin(), name.begin() + 64, [](unsigned char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

void Walk(const fs::path &directory, Result &result, bool clear) {
  std::error_code ec;
  if (!IsSafeDirectory(directory)) {
    Error(result, directory, "Unsafe or inaccessible directory");
    return;
  }
  if (!fs::exists(directory, ec)) {
    if (ec) Error(result, directory, ec.message());
    return;
  }
  fs::directory_iterator it(directory, ec), end;
  if (ec) Error(result, directory, ec.message());
  while (!ec && it != end) {
    const auto path = it->path();
    const auto status = it->symlink_status(ec);
    if (ec) { Error(result, path, ec.message()); break; }
    if (fs::is_symlink(status)) {
      Error(result, path, "Symbolic link skipped");
    } else if (fs::is_directory(status)) {
      if (clear && !IsOwnedCacheDirectory(path)) {
        Error(result, path, "Unrecognized cache directory preserved");
      } else {
        Walk(path, result, clear);
        if (clear) {
          fs::remove(path, ec);
          if (ec) Error(result, path, ec.message());
        }
      }
    } else if (fs::is_regular_file(status)) {
      if (clear) {
        if (IsOwnedCacheFile(path)) {
          if (fs::remove(path, ec)) ++result.removedFiles;
        } else {
          Error(result, path, "Unrecognized cache file preserved");
        }
      } else {
        const auto size = fs::file_size(path, ec);
        if (!ec) result.bytes += size;
      }
      if (ec) Error(result, path, ec.message());
    } else {
      Error(result, path, "Unsupported cache entry");
    }
    ec.clear();
    it.increment(ec);
    if (ec) Error(result, directory, ec.message());
  }
}

bool IsLegacyCache(const fs::path &path) {
  if (path.extension() != ".cache") return false;
  auto source = path;
  source.replace_extension();
  auto ext = source.extension().string();
  if (ext.size() > 1 && ext.size() <= 21 &&
      std::all_of(ext.begin() + 1, ext.end(), [](unsigned char c) {
        return c >= '0' && c <= '9';
      })) {
    source.replace_extension();
    ext = source.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    if (ext != ".gdtf") return false;
  }
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  const bool gdtf = ext == ".gdtf";
  if (!gdtf && ext != ".3ds" && ext != ".glb" &&
      ext != ".obj") return false;
  std::error_code ec;
  if (!fs::is_regular_file(fs::symlink_status(source, ec)) || ec) return false;
  std::ifstream input(path, std::ios::binary);
  uint32_t magic = 0, version = 0;
  input.read(reinterpret_cast<char *>(&magic), sizeof(magic));
  input.read(reinterpret_cast<char *>(&version), sizeof(version));
  return input.good() && magic == (gdtf ? 0x48434747u : 0x4843534Du) &&
         version >= 1 && version <= 4 && fs::file_size(path, ec) >= 24 && !ec;
}
} // namespace

bool IsSafeDirectory(const fs::path &directory) {
  if (directory.empty()) return false;
  std::error_code ec;
  auto absolute = fs::absolute(directory, ec).lexically_normal();
  if (ec) return false;
  // Platform-provided parent directories may contain system aliases (for
  // example /tmp on macOS). Trust those parents, but never links inside the
  // application-owned tree. External legacy roots are checked at their leaf;
  // cleanup checks the library root separately and never follows child links.
  const auto data = fs::absolute(AppPaths::GetUserDataDir(), ec).lexically_normal();
  if (ec) return false;
  const auto relative = absolute.lexically_relative(data);
  const bool inUserData = !relative.empty() && *relative.begin() != "..";
  const auto parent = inUserData ? data.parent_path() : absolute.parent_path();
  auto current = fs::weakly_canonical(parent, ec);
  if (ec) return false;
  const auto tail = absolute.lexically_relative(parent);
  for (const auto &component : tail) {
    current /= component;
    const auto status = fs::symlink_status(current, ec);
    if (ec == std::errc::no_such_file_or_directory) { ec.clear(); continue; }
    if (ec || fs::is_symlink(status) ||
        (fs::exists(status) && !fs::is_directory(status))) return false;
  }
  return true;
}

bool IsSafeCacheFile(const fs::path &file) {
  if (!IsSafePreparedParent(file.parent_path())) return false;
  std::error_code ec;
  const auto status = fs::symlink_status(file, ec);
  return ec == std::errc::no_such_file_or_directory ||
         (!ec && (!fs::exists(status) || fs::is_regular_file(status)));
}

bool EnsureDirectories() {
  if (activeBatch) return activeBatch->prepared;
  performance::Count(performance::Work::CacheRoot);
  for (const auto &dir : {AppPaths::GetViewer3DGdtfCacheDir(),
                          AppPaths::GetViewer3DMeshCacheDir(),
                          AppPaths::GetGdtfCatalogCachePath().parent_path()}) {
    std::error_code ec;
    if (!IsSafeDirectory(dir)) return false;
    fs::create_directories(dir, ec);
    if (ec) return false;
  }
  return true;
}

Result GetStatistics() {
  Result result;
  Walk(AppPaths::GetCacheRoot(), result, false);
  return result;
}

bool MarkCatalogMigrationComplete() {
  const auto marker = AppPaths::GetCatalogCacheMigrationMarker();
  std::error_code ec;
  if (!IsSafeDirectory(marker.parent_path())) return false;
  fs::create_directories(marker.parent_path(), ec);
  if (ec || fs::is_symlink(fs::symlink_status(marker, ec))) return false;
  return static_cast<bool>(std::ofstream(marker, std::ios::trunc));
}

Result Clear() {
  Result result;
  // Record the explicit cache discard even when an old catalog cannot be removed.
  if (!MarkCatalogMigrationComplete()) {
    Error(result, AppPaths::GetCatalogCacheMigrationMarker(), "Cannot record cache discard");
    return result;
  }
  Walk(AppPaths::GetCacheRoot(), result, true);
  if (!EnsureDirectories())
    Error(result, AppPaths::GetCacheRoot(), "Cannot recreate cache directories");
  return result;
}

fs::path ViewerCachePath(const std::string &sourcePath,
                         const std::string &mode, bool gdtf) {
  std::string identity;
  if (activeBatch) {
    const auto it = activeBatch->identities.find(sourcePath);
    if (it != activeBatch->identities.end()) identity = it->second;
  }
  if (identity.empty()) {
    const auto packaged = packaged_resources::Lookup(PathUtils::PathFromUtf8(sourcePath));
    identity = packaged.value_or("");
    if (identity.empty()) identity = PathUtils::BuildFilesystemIdentityKey(sourcePath);
    if (activeBatch) activeBatch->identities[sourcePath] = identity;
  }
  // Length-delimit identity and mode to avoid ambiguous token concatenations.
  const auto token = std::to_string(identity.size()) + ":" + identity +
                     std::to_string(mode.size()) + ":" + mode;
  return (gdtf ? AppPaths::GetViewer3DGdtfCacheDir()
               : AppPaths::GetViewer3DMeshCacheDir()) /
         (FileImportUtils::ComputeBytesSha256(token.data(), token.size()) + ".cache");
}

ViewerCacheBatch::ViewerCacheBatch() : previous(activeBatch) {
  prepared = previous ? previous->prepared : EnsureDirectories();
  directories = {AppPaths::GetViewer3DGdtfCacheDir(), AppPaths::GetViewer3DMeshCacheDir()};
  activeBatch = this;
}
ViewerCacheBatch::~ViewerCacheBatch() { activeBatch = previous; }

Result CleanupLegacyViewerCaches(const fs::path &libraryRoot) {
  Result result;
  if (libraryRoot.empty() || !IsSafeDirectory(libraryRoot)) {
    Error(result, libraryRoot, "Unsafe or inaccessible library root");
    return result;
  }
  for (const auto *scope : {"fixtures", "trusses", "scene_objects", "hoists"}) {
    const auto directory = libraryRoot / scope;
    std::error_code ec;
    if (!IsSafeDirectory(directory)) {
      Error(result, directory, "Unsafe or inaccessible library directory");
      continue;
    }
    if (!fs::exists(directory, ec)) {
      if (ec) Error(result, directory, ec.message());
      continue;
    }
    fs::recursive_directory_iterator it(directory, ec), end;
    if (ec) Error(result, directory, ec.message());
    while (!ec && it != end) {
      const auto path = it->path();
      const auto status = it->symlink_status(ec);
      if (ec) { Error(result, path, ec.message()); break; }
      if (fs::is_regular_file(status) && IsLegacyCache(path)) {
        if (fs::remove(path, ec)) ++result.removedFiles;
        if (ec) Error(result, path, ec.message());
      }
      ec.clear();
      it.increment(ec);
      if (ec) Error(result, directory, ec.message());
    }
  }
  return result;
}
} // namespace CacheService
