#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace LibraryBootstrap {

struct BootstrapResult {
  bool attempted = false;
  bool completed = false;
  size_t filesCopied = 0;
  size_t filesSkippedExisting = 0;
  size_t directoriesCreated = 0;
  std::vector<std::string> errors;
};

// Bootstraps userData/library from the installed library tree.
// The operation is non-destructive and copies only missing files.
BootstrapResult BootstrapUserLibrary(const std::filesystem::path &installedLibraryRoot,
                                     const std::filesystem::path &userDataDir);

} // namespace LibraryBootstrap
