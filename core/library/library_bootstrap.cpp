#include "library/library_bootstrap.h"

#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace LibraryBootstrap {
namespace {

constexpr const char *kBootstrapVersion = "1";
constexpr const char *kBootstrapStampFilename = ".bootstrap_version";

bool CopyMissingFilesRecursively(const fs::path &sourceRoot, const fs::path &destinationRoot) {
  std::error_code ec;
  if (!fs::exists(sourceRoot, ec) || ec || !fs::is_directory(sourceRoot, ec) || ec)
    return false;

  fs::create_directories(destinationRoot, ec);
  if (ec)
    return false;

  for (const fs::directory_entry &entry :
       fs::recursive_directory_iterator(sourceRoot, fs::directory_options::skip_permission_denied,
                                        ec)) {
    if (ec)
      return false;

    const fs::path relative = fs::relative(entry.path(), sourceRoot, ec);
    if (ec)
      return false;

    const fs::path destinationPath = destinationRoot / relative;
    if (entry.is_directory()) {
      fs::create_directories(destinationPath, ec);
      if (ec)
        return false;
      continue;
    }

    if (!entry.is_regular_file())
      continue;

    if (fs::exists(destinationPath, ec)) {
      if (ec)
        return false;
      continue;
    }

    fs::create_directories(destinationPath.parent_path(), ec);
    if (ec)
      return false;

    fs::copy_file(entry.path(), destinationPath, fs::copy_options::none, ec);
    if (ec)
      return false;
  }

  return true;
}

std::string ReadBootstrapStamp(const fs::path &stampPath) {
  std::ifstream input(stampPath);
  if (!input.is_open())
    return {};

  std::string version;
  std::getline(input, version);
  return version;
}

bool WriteBootstrapStamp(const fs::path &stampPath) {
  std::ofstream output(stampPath, std::ios::out | std::ios::trunc);
  if (!output.is_open())
    return false;

  output << kBootstrapVersion;
  return true;
}

} // namespace

BootstrapResult BootstrapUserLibrary(const fs::path &installedLibraryRoot,
                                     const fs::path &userDataDir) {
  BootstrapResult result;

  std::error_code ec;
  if (installedLibraryRoot.empty() || userDataDir.empty())
    return result;

  if (!fs::exists(installedLibraryRoot, ec) || ec || !fs::is_directory(installedLibraryRoot, ec) ||
      ec)
    return result;

  const fs::path userLibraryRoot = userDataDir / "library";
  fs::create_directories(userLibraryRoot, ec);
  if (ec)
    return result;

  const fs::path stampPath = userLibraryRoot / kBootstrapStampFilename;
  const bool needsBootstrap = ReadBootstrapStamp(stampPath) != kBootstrapVersion;
  if (!needsBootstrap)
    return result;

  result.attempted = true;
  if (!CopyMissingFilesRecursively(installedLibraryRoot, userLibraryRoot))
    return result;

  if (!WriteBootstrapStamp(stampPath))
    return result;

  result.completed = true;
  return result;
}

} // namespace LibraryBootstrap
