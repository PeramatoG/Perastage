#pragma once

#include <filesystem>

namespace LibraryBootstrap {

struct BootstrapResult {
  bool attempted = false;
  bool completed = false;
};

// Bootstraps userData/library from the installed library tree.
// The operation is non-destructive and copies only missing files.
BootstrapResult BootstrapUserLibrary(const std::filesystem::path &installedLibraryRoot,
                                     const std::filesystem::path &userDataDir);

} // namespace LibraryBootstrap
