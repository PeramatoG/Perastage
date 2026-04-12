#include "library/library_bootstrap.h"

#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace LibraryBootstrap {
namespace {

bool CopyMissingFilesRecursively(const fs::path &sourceRoot, const fs::path &destinationRoot,
                                 BootstrapResult &result) {
  std::error_code ec;
  if (!fs::exists(sourceRoot, ec) || ec || !fs::is_directory(sourceRoot, ec) || ec) {
    std::ostringstream oss;
    oss << "Installed library root is not available: " << sourceRoot.string();
    result.errors.push_back(oss.str());
    return false;
  }

  ec.clear();
  const bool createdRoot = fs::create_directories(destinationRoot, ec);
  if (ec) {
    std::ostringstream oss;
    oss << "Could not create destination root '" << destinationRoot.string()
        << "': " << ec.message();
    result.errors.push_back(oss.str());
    return false;
  }
  if (createdRoot)
    ++result.directoriesCreated;

  for (const fs::directory_entry &entry :
       fs::recursive_directory_iterator(sourceRoot, fs::directory_options::skip_permission_denied,
                                        ec)) {
    if (ec) {
      std::ostringstream oss;
      oss << "Failed while traversing '" << sourceRoot.string() << "': " << ec.message();
      result.errors.push_back(oss.str());
      return false;
    }

    const fs::path relative = fs::relative(entry.path(), sourceRoot, ec);
    if (ec) {
      std::ostringstream oss;
      oss << "Failed to compute relative path for '" << entry.path().string()
          << "': " << ec.message();
      result.errors.push_back(oss.str());
      return false;
    }

    const fs::path destinationPath = destinationRoot / relative;
    if (entry.is_directory()) {
      ec.clear();
      const bool createdDirectory = fs::create_directories(destinationPath, ec);
      if (ec) {
        std::ostringstream oss;
        oss << "Could not create directory '" << destinationPath.string()
            << "': " << ec.message();
        result.errors.push_back(oss.str());
        return false;
      }
      if (createdDirectory)
        ++result.directoriesCreated;
      continue;
    }

    if (!entry.is_regular_file())
      continue;

    ec.clear();
    if (fs::exists(destinationPath, ec)) {
      if (ec) {
        std::ostringstream oss;
        oss << "Could not check existing destination '" << destinationPath.string()
            << "': " << ec.message();
        result.errors.push_back(oss.str());
        return false;
      }
      ++result.filesSkippedExisting;
      continue;
    }

    ec.clear();
    fs::create_directories(destinationPath.parent_path(), ec);
    if (ec) {
      std::ostringstream oss;
      oss << "Could not create parent directory '" << destinationPath.parent_path().string()
          << "': " << ec.message();
      result.errors.push_back(oss.str());
      return false;
    }

    ec.clear();
    fs::copy_file(entry.path(), destinationPath, fs::copy_options::none, ec);
    if (ec) {
      std::ostringstream oss;
      oss << "Could not copy '" << entry.path().string() << "' to '" << destinationPath.string()
          << "': " << ec.message();
      result.errors.push_back(oss.str());
      return false;
    }
    ++result.filesCopied;
  }

  return true;
}

} // namespace

BootstrapResult BootstrapUserLibrary(const fs::path &installedLibraryRoot,
                                     const fs::path &userDataDir) {
  BootstrapResult result;
  result.attempted = true;

  std::error_code ec;
  if (installedLibraryRoot.empty() || userDataDir.empty()) {
    result.errors.emplace_back("Bootstrap skipped because installed or user-data path is empty.");
    return result;
  }

  if (!fs::exists(installedLibraryRoot, ec) || ec || !fs::is_directory(installedLibraryRoot, ec) ||
      ec) {
    result.errors.emplace_back("Bootstrap skipped because installed library root is missing.");
    return result;
  }

  const fs::path userLibraryRoot = userDataDir / "library";
  ec.clear();
  fs::create_directories(userLibraryRoot, ec);
  if (ec) {
    std::ostringstream oss;
    oss << "Could not create user library root '" << userLibraryRoot.string()
        << "': " << ec.message();
    result.errors.push_back(oss.str());
    return result;
  }

  if (!CopyMissingFilesRecursively(installedLibraryRoot, userLibraryRoot, result))
    return result;

  result.completed = true;
  return result;
}

} // namespace LibraryBootstrap
