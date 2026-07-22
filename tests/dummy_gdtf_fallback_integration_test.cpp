#include "gdtf_canonicalizer.h"
#include "wx_path_utils.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

// Reports a test failure with context for CI logs.
static bool Fail(const std::string &message) {
  std::cerr << "ERROR: " << message << std::endl;
  return false;
}

// Creates a unique temporary directory for this test process.
static fs::path CreateTempDir() {
  const fs::path base = fs::temp_directory_path();
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path path = base / ("perastage_dummy_fallback_integration_" +
                            std::to_string(stamp) + "_" + std::to_string(attempt));
    std::error_code ec;
    if (fs::create_directories(path, ec) && !ec)
      return path;
  }
  return {};
}

// Removes a temporary directory at scope exit.
class ScopedTempDir {
public:
  ScopedTempDir() : path(CreateTempDir()) {}
  ~ScopedTempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  fs::path path;
};

// Reads the single description.xml entry from an archive.
static bool ArchiveHasRootDescriptionOnly(const fs::path &archivePath) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(archivePath));
  if (!input.IsOk())
    return Fail("Could not open archive: " + archivePath.string());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  int count = 0;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    ++count;
    if (entry->GetName().ToStdString() != "description.xml")
      return Fail("Unexpected GDTF entry after canonicalizer/export path: " +
                  entry->GetName().ToStdString());
  }
  return count == 1 || Fail("Expected exactly one GDTF entry after canonicalizer/export path");
}

// Verifies the generated fallback passes production validation and copy/export-style canonicalization.
int main(int argc, char **argv) {
  wxInitializer initializer;
  if (!initializer.IsOk())
    return Fail("wxWidgets initialization failed") ? 0 : 1;
  if (argc != 2)
    return Fail("usage: dummy_gdtf_fallback_integration_test <Dummy 1ch.gdtf>") ? 0 : 1;

  const fs::path fallbackPath = argv[1];
  if (!fs::is_regular_file(fallbackPath))
    return Fail("Generated fallback archive is missing: " + fallbackPath.string()) ? 0 : 1;

  const auto validation = GdtfCanonicalizer::ValidateArchive(fallbackPath);
  if (!validation.success)
    return Fail("Generated fallback failed production validation") ? 0 : 1;

  ScopedTempDir temp;
  if (temp.path.empty())
    return Fail("Could not create temporary directory") ? 0 : 1;
  const fs::path exported = temp.path / "Dummy 1ch exported.gdtf";
  const auto canonicalized = GdtfCanonicalizer::CanonicalizeArchive(fallbackPath, exported);
  if (!canonicalized.success)
    return Fail("Generated fallback failed production canonicalization/export path") ? 0 : 1;
  if (!fs::is_regular_file(exported))
    return Fail("Canonicalized/exported fallback archive was not written") ? 0 : 1;
  const auto exportedValidation = GdtfCanonicalizer::ValidateArchive(exported);
  if (!exportedValidation.success)
    return Fail("Canonicalized/exported fallback archive is invalid") ? 0 : 1;
  if (!ArchiveHasRootDescriptionOnly(exported))
    return 1;
  return 0;
}
