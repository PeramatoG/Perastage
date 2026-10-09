#include "fixture_gdtf_derivative_publication.h"

#include "fixture_gdtf_derivative_contract.h"
#include "file_import_utils.h"
#include "filesystem_path_utils.h"
#include "gdtf_archive_reader.h"
#include "gdtf_canonicalizer.h"
#include "gdtf_filename_policy.h"
#include "gdtf_mutation_audit.h"
#include "gdtf_publication_resources.h"
#include "symbols/fixture_symbol_resource_contract.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <system_error>
#include <vector>

namespace fixture_gdtf {
namespace {

bool IsOwnedProjectDerivative(const std::filesystem::path &source,
                              const std::filesystem::path &fixtureDirectory) {
  std::error_code ec;
  if (!std::filesystem::equivalent(source.parent_path(), fixtureDirectory, ec) ||
      ec || !gdtf_filename_policy::IsPerastageNamedFile(source))
    return false;
  if (HasPerastageStandardSvgRevision(source))
    return true;
  FixtureSymbolResourceInspection legacy;
  return InspectFixtureSymbolResources(PathUtils::PathToUtf8(source), legacy) &&
         legacy.perastageViewsUsable;
}

std::filesystem::path CollisionName(const std::filesystem::path &requested,
                                    const std::string &hash, unsigned index) {
  std::string name = requested.filename().string();
  const auto marker = name.rfind("@Perastage");
  const std::string suffix = "_symbols_" + hash.substr(0, 12) +
                            (index ? "_" + std::to_string(index + 1) : "");
  if (marker != std::string::npos)
    name.insert(marker, suffix);
  else
    name = requested.stem().string() + suffix + requested.extension().string();
  return requested.parent_path() / name;
}

bool PrepareStandardPublicationInput(const std::filesystem::path &source,
                                     const std::filesystem::path &destination,
                                     bool &rewritten,
                                     std::string &errorMessage) {
  rewritten = false;
  std::vector<GdtfCanonicalizer::ResourceMutation> mutations;
  if (!BuildStandardPublicationResourceMutations(source, mutations, errorMessage))
    return false;
  if (mutations.empty())
    return true;
  const auto rewrite = GdtfCanonicalizer::RewriteArchiveResources(
      source, destination, mutations, [](tinyxml2::XMLDocument &document) {
        auto *fixture = GdtfMutationAudit::EnsureFixtureType(document);
        if (!fixture)
          return false;
        GdtfMutationAudit::AppendRevision(
            fixture, document, "Removed legacy Perastage private symbol extensions",
            GdtfMutationAudit::BuildPerastageModifiedBy(), 0, "",
            GdtfMutationAudit::RevisionPolicy::RecordEffectiveChange);
        return true;
      });
  if (!rewrite.success) {
    errorMessage = rewrite.errors.empty()
                       ? "Could not remove legacy private publication resources."
                       : rewrite.errors.front();
    return false;
  }
  rewritten = true;
  return true;
}

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

// Builds deterministic canonicalization options for Perastage-owned output.
GdtfCanonicalizer::Options BuildPublicationCanonicalizationOptions(
    const std::filesystem::path &sourceLabel) {
  GdtfCanonicalizer::Options options;
  options.allowPlaceholderFixtureTypeIdRepair = true;
  options.stableIdSeed = "Perastage canonical GDTF publication";
  options.sourceLabel = sourceLabel.filename().string();
  return options;
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

bool HasPerastageStandardSvgRevision(const std::filesystem::path &sourcePath) {
  const auto archive = gdtf::ReadGdtfArchive(sourcePath);
  if (!archive.Success())
    return false;
  tinyxml2::XMLDocument document;
  if (document.Parse(archive.descriptionXml.c_str(), archive.descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS)
    return false;
  const auto *root = document.FirstChildElement("GDTF");
  const auto *fixture = root ? root->FirstChildElement("FixtureType") : nullptr;
  const auto *revisions = fixture ? fixture->FirstChildElement("Revisions") : nullptr;
  for (const auto *revision = revisions ? revisions->FirstChildElement("Revision") : nullptr;
       revision; revision = revision->NextSiblingElement("Revision")) {
    const char *modifiedBy = revision->Attribute("ModifiedBy");
    const char *text = revision->Attribute("Text");
    if (modifiedBy && text && IsPerastageStandardSvgMutationRevision(modifiedBy, text))
      return true;
  }
  return false;
}

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
  fs::path sanitizedPath = workingPath;
  sanitizedPath += ".standard-input";
  bool sanitized = false;
  if (!PrepareStandardPublicationInput(sourcePath, sanitizedPath, sanitized,
                                        errorMessage)) {
    fs::remove(sanitizedPath, ec);
    return false;
  }
  const GdtfCanonicalizer::Options options =
      BuildPublicationCanonicalizationOptions(sourcePath);
  const GdtfCanonicalizer::Result canonical =
      GdtfCanonicalizer::CanonicalizeArchive(sanitized ? sanitizedPath : sourcePath,
                                             workingPath, options);
  fs::remove(sanitizedPath, ec);
  if (!canonical.success) {
    fs::remove(workingPath, ec);
    errorMessage = canonical.errors.empty()
                       ? "GDTF canonicalization failed."
                       : canonical.errors.front();
    return false;
  }
  if (!ValidatePublishedDerivative(workingPath.string(), errorMessage)) {
    fs::remove(workingPath, ec);
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
  const fs::path fixtureDirectory = projectBasePath / "fixtures";
  fs::path publishedPath = fixtureDirectory / canonicalFileName.filename();
  if (IsOwnedProjectDerivative(sourcePath, fixtureDirectory)) {
    publishedPath = sourcePath;
  } else {
    std::error_code ec;
    if (fs::exists(publishedPath, ec)) {
      const auto hash = FileImportUtils::ComputeFileSha256(sourcePath);
      if (!hash) {
        errorMessage = "Could not fingerprint the original fixture before derivative publication.";
        return false;
      }
      for (unsigned index = 0; ; ++index) {
        const fs::path candidate = CollisionName(publishedPath, *hash, index);
        if (!fs::exists(candidate, ec)) {
          publishedPath = candidate;
          break;
        }
        if (ec || index >= 1023) {
          errorMessage = "Could not allocate a distinct fixture derivative publication path.";
          return false;
        }
      }
    }
    if (ec) {
      errorMessage = "Could not inspect the fixture derivative publication destination.";
      return false;
    }
  }
  return PrepareOwnedDerivative(sourcePath, publishedPath,
                                NormalizeReference((fs::path("fixtures") /
                                    publishedPath.filename()).string()),
                                prepared, errorMessage);
}

bool PrepareOwnedDerivative(const std::filesystem::path &sourcePath,
                            const std::filesystem::path &publishedPath,
                            const std::string &publishedReference,
                            PreparedDerivative &prepared,
                            std::string &errorMessage) {
  namespace fs = std::filesystem;
  prepared = {};
  if (sourcePath.empty() || publishedPath.empty() || publishedReference.empty()) {
    errorMessage = "A source and owned destination are required to prepare a derivative.";
    return false;
  }
  std::error_code ec;
  fs::create_directories(publishedPath.parent_path(), ec);
  if (ec) {
    errorMessage = "Could not create the owned fixture derivative directory.";
    return false;
  }
  prepared.publishedPath = publishedPath;
  prepared.publishedReference = publishedReference;
  static std::atomic<unsigned long long> nextWorkingId{0};
  prepared.workingPath = publishedPath;
  prepared.workingPath += ".working." + std::to_string(nextWorkingId.fetch_add(1));
  fs::copy_file(sourcePath, prepared.workingPath,
                fs::copy_options::overwrite_existing, ec);
  if (ec) {
    prepared = {};
    errorMessage = "Could not prepare the private fixture GDTF working derivative.";
    return false;
  }
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
  const fs::path canonicalWorkingPath =
      BuildCanonicalWorkingPath(prepared.publishedPath);
  fs::path sanitizedPath = canonicalWorkingPath;
  sanitizedPath += ".standard-input";
  bool sanitized = false;
  if (!PrepareStandardPublicationInput(prepared.workingPath, sanitizedPath,
                                        sanitized, errorMessage)) {
    DiscardPreparedDerivative(prepared);
    fs::remove(sanitizedPath);
    return false;
  }
  const GdtfCanonicalizer::Options options =
      BuildPublicationCanonicalizationOptions(prepared.publishedPath);
  const GdtfCanonicalizer::Result canonical =
      GdtfCanonicalizer::CanonicalizeArchive(sanitized ? sanitizedPath : prepared.workingPath,
                                             canonicalWorkingPath, options);
  fs::remove(prepared.workingPath);
  fs::remove(sanitizedPath);
  if (!canonical.success) {
    fs::remove(canonicalWorkingPath);
    errorMessage = canonical.errors.empty()
                       ? "Fixture derivative canonicalization failed."
                       : canonical.errors.front();
    return false;
  }
  if (!ValidatePublishedDerivative(canonicalWorkingPath.string(), errorMessage)) {
    fs::remove(canonicalWorkingPath);
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
