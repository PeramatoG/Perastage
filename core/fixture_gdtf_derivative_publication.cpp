#include "fixture_gdtf_derivative_publication.h"

#include "fixture_gdtf_derivative_contract.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <system_error>

namespace fixture_gdtf {
namespace {

// Normalizes project references to portable forward-slash separators.
std::string NormalizeReference(std::string reference) {
  std::replace(reference.begin(), reference.end(), '\\', '/');
  return reference;
}

} // namespace

// Prepares a private working copy and its eventual project publication target.
bool PrepareProjectDerivative(const std::filesystem::path &sourcePath,
                              const std::filesystem::path &projectBasePath,
                              const std::filesystem::path &canonicalFileName,
                              PreparedDerivative &prepared,
                              std::string &errorMessage) {
  namespace fs = std::filesystem;
  prepared = {};
  if (sourcePath.empty() || projectBasePath.empty() ||
      canonicalFileName.empty()) {
    errorMessage = "A source GDTF and project folder are required to prepare a derivative.";
    return false;
  }
  std::error_code ec;
  const fs::path fixtureDirectory = projectBasePath / "fixtures";
  fs::create_directories(fixtureDirectory, ec);
  if (ec) {
    errorMessage = "Could not create the project fixture derivative directory.";
    return false;
  }
  prepared.publishedPath = fixtureDirectory / canonicalFileName.filename();
  static std::atomic<unsigned long long> nextWorkingId{0};
  prepared.workingPath = prepared.publishedPath;
  prepared.workingPath += ".working." +
                          std::to_string(nextWorkingId.fetch_add(1));
  fs::copy_file(sourcePath, prepared.workingPath,
                fs::copy_options::overwrite_existing, ec);
  if (ec) {
    prepared = {};
    errorMessage = "Could not prepare the private fixture GDTF working derivative.";
    return false;
  }
  const fs::path relative = fs::relative(prepared.publishedPath,
                                         projectBasePath, ec);
  if (ec) {
    DiscardPreparedDerivative(prepared);
    prepared = {};
    errorMessage = "Could not create the project-relative derivative reference.";
    return false;
  }
  prepared.publishedReference = NormalizeReference(relative.string());
  errorMessage.clear();
  return true;
}

// Validates and atomically publishes a prepared canonical derivative.
bool PublishPreparedDerivative(const PreparedDerivative &prepared,
                               std::string &errorMessage) {
  namespace fs = std::filesystem;
  if (prepared.workingPath.empty() || prepared.publishedPath.empty() ||
      prepared.publishedReference.empty()) {
    errorMessage = "Fixture derivative publication was not prepared.";
    return false;
  }
  if (!ValidatePublishedDerivative(prepared.workingPath.string(), errorMessage)) {
    errorMessage = "Fixture derivative publication validation failed: " + errorMessage;
    DiscardPreparedDerivative(prepared);
    return false;
  }
  std::error_code ec;
#ifdef _WIN32
  const BOOL moved = MoveFileExW(prepared.workingPath.wstring().c_str(),
                                 prepared.publishedPath.wstring().c_str(),
                                 MOVEFILE_REPLACE_EXISTING |
                                     MOVEFILE_WRITE_THROUGH);
  if (!moved)
    ec = std::error_code(static_cast<int>(GetLastError()),
                         std::system_category());
#else
  fs::rename(prepared.workingPath, prepared.publishedPath, ec);
#endif
  if (!ec) {
    errorMessage.clear();
    return true;
  }
  DiscardPreparedDerivative(prepared);
  errorMessage = "Could not atomically publish the validated fixture derivative.";
  return false;
}

// Removes private working storage without touching a published derivative.
void DiscardPreparedDerivative(const PreparedDerivative &prepared) {
  std::error_code ignored;
  if (!prepared.workingPath.empty())
    std::filesystem::remove(prepared.workingPath, ignored);
}

} // namespace fixture_gdtf
