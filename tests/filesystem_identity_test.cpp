#include "filesystem_path_utils.h"
#include "trussloader.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// Writes a small temporary file used by path identity regression checks.
static fs::path WriteTempFile(const fs::path &path) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << "identity";
  return path;
}

// Runs filesystem identity checks for spaces and UTF-8 path spelling.
int main() {
  const fs::path root = fs::temp_directory_path() / "perastage_identity_test_å";
  fs::remove_all(root);
  const fs::path path = WriteTempFile(root / "Folder With Spaces" / "Sample File.gdtf");

  const std::string key = PathUtils::BuildFilesystemIdentityKey(path);
  assert(!key.empty());
  assert(key.find('\\') == std::string::npos);

  const std::string relativeKey = PathUtils::BuildFilesystemIdentityKey(
      fs::path("Folder With Spaces") / "Sample File.gdtf", root);
  assert(key == relativeKey);

#ifdef _WIN32
  fs::path upper = path.parent_path() / "SAMPLE FILE.gdtf";
  assert(PathUtils::BuildFilesystemIdentityKey(upper) == key);
  assert(BuildGdtfExtractionCacheDirForTesting(upper) ==
         BuildGdtfExtractionCacheDirForTesting(path));
#else
  assert(BuildGdtfExtractionCacheDirForTesting(path) ==
         BuildGdtfExtractionCacheDirForTesting(path));
#endif

  fs::remove_all(root);
  return 0;
}
