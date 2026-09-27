#include "fixture_gdtf_derivative_publication.h"

#include "fixture_gdtf_derivative_contract.h"
#include "gdtf_canonicalizer.h"

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

// Returns a collision-resistant private sibling path for canonicalization.
std::filesystem::path BuildCanonicalWorkingPath(
    const std::filesystem::path &publishedPath) {
  static std::atomic<unsigned long long> nextWorkingId{0};
  std::filesystem::path workingPath = publishedPath;
  workingPath += ".canonical-working." +
                 std::to_string(nextWorkingId.fetch_add(1));
  return workingPath;
}

// Replaces a published archive atomically with a completed private archive.
bool ReplacePublishedArchive(const std::filesystem::path &workingPath,
                             const std::filesystem::path &publishedPath,
                             std::string &errorMessage) {
  std::error_code ec;
#ifdef _WIN32
  const BOOL moved = MoveFileExW(workingPath.wstring().c_str(),
                                 publishedPath.wstring().c_str(),
                                 MOVEFILE_REPLACE_EXISTING |
                                     MOVEFILE_WRITE_THROUGH);
  if (!moved)
    ec = std::error_code(static_cast<int>(GetLastError()),
                         std::system_category());
#else
  std::filesystem::rename(workingPath, publishedPath, ec);
#endif
  if (!ec) {
    errorMessage.clear();
    return true;
  }
  std::filesystem::remove(workingPath, ec);
  errorMessage = "Could not atomically publish the canonical fixture GDTF.";
  return false;
}

} // namespace

// Canonicalizes a source archive through a private copy and atomically publishes it.
bool PublishCanonicalGdtfCopy(const std::filesystem::path &sourcePath,
                              const std::filesystem::path &publishedPath,
                              std::string &errorMessage) {
  namespace fs = std::filesystem;
  if (sourcePath.empty() || publishedPath.empty()) {
    errorMessage = "A source and destination are required for GDTF publication.";
    return false;
  }
  std::error_code ec;
  if (!fs::is_regular_file(sourcePath, ec) || ec) {
    errorMessage = "The GDTF publication source does not exist or is not a file.";
    return false;
  }
  std::error_code sourcePathError;
  const fs::path absoluteSource =
      fs::absolute(sourcePath, sourcePathError).lexically_normal();
  std::error_code destinationPathError;
  const fs::path absoluteDestination =
      fs::absolute(publishedPath, destinationPathError).lexically_normal();
  if ((fs::exists(publishedPath, ec) && !ec &&
       fs::equivalent(sourcePath, publishedPath, ec)) ||
      (!sourcePathError && !destinationPathError &&
       absoluteSource == absoluteDestination)) {
    errorMessage = "Canonical publication requires a destination copy distinct from the source.";
    return false;
  }
  ec.clear();
  if (!publishedPath.parent_path().empty())
    fs::create_directories(publishedPath.parent_path(), ec);
  if (ec) {
    errorMessage = "Could not create the canonical GDTF destination directory.";
    return false;
  }
  const fs::path workingPath = BuildCanonicalWorkingPath(publishedPath);
  const GdtfCanonicalizer::Result canonical =
      GdtfCanonicalizer::CanonicalizeArchive(sourcePath, workingPath);
  if (!canonical.success) {
    fs::remove(workingPath, ec);
    errorMessage = canonical.errors.empty()
                       ? "GDTF canonicalization failed."
                       : canonical.errors.front();
    return false;
  }
  return ReplacePublishedArchive(workingPath, publishedPath, errorMessage);
}

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
  const fs::path canonicalWorkingPath =
      BuildCanonicalWorkingPath(prepared.publishedPath);
  const GdtfCanonicalizer::Result canonical =
      GdtfCanonicalizer::CanonicalizeArchive(prepared.workingPath,
                                             canonicalWorkingPath);
  fs::remove(prepared.workingPath);
  if (!canonical.success) {
    fs::remove(canonicalWorkingPath);
    errorMessage = canonical.errors.empty()
                       ? "Fixture derivative canonicalization failed."
                       : canonical.errors.front();
    return false;
  }
  if (ReplacePublishedArchive(canonicalWorkingPath, prepared.publishedPath,
                              errorMessage))
    return true;
  DiscardPreparedDerivative(prepared);
  return false;
}

// Removes private working storage without touching a published derivative.
void DiscardPreparedDerivative(const PreparedDerivative &prepared) {
  std::error_code ignored;
  if (!prepared.workingPath.empty())
    std::filesystem::remove(prepared.workingPath, ignored);
}

} // namespace fixture_gdtf
