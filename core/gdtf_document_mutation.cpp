#include "gdtf_document_mutation.h"

#include "filesystem_path_utils.h"
#include "gdtf_archive_reader.h"
#include "gdtf_canonicalizer.h"
#include "gdtf_mutation_audit.h"
#include "runtime_storage.h"

#include <tinyxml2.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <sstream>
#include <system_error>
#include <vector>

namespace gdtf {
namespace {

namespace fs = std::filesystem;

struct ArchivePublicationResult {
  bool success = false;
  std::string stage;
  std::string diagnostic;
  fs::path tempPath;
};

// Reports whether an extraction diagnostic prevents document mutation.
bool IsFatalExtractionDiagnostic(const ArchiveDiagnostic &diagnostic) {
  switch (diagnostic.code) {
  case ArchiveDiagnosticCode::None:
  case ArchiveDiagnosticCode::NonCanonicalDescriptionXml:
  case ArchiveDiagnosticCode::Utf8FlagMissing:
  case ArchiveDiagnosticCode::Utf8FallbackUsed:
  case ArchiveDiagnosticCode::LegacyFilenameEncodingUsed:
    return false;
  default:
    return true;
  }
}

// Extracts an archive into an automatically cleaned Core-owned workspace.
class MutationWorkspace {
public:
  // Creates and populates the temporary mutation workspace.
  explicit MutationWorkspace(const std::string &archivePath)
      : workspace_("gdtf-mutation") {
    if (!workspace_.IsValid())
      return;
    const ArchiveReadResult extraction = ExtractGdtfArchive(
        PathUtils::PathFromUtf8(archivePath), workspace_.Path());
    valid_ = !extraction.entries.empty() &&
             std::none_of(extraction.diagnostics.begin(),
                          extraction.diagnostics.end(),
                          IsFatalExtractionDiagnostic);
  }

  // Returns whether archive extraction completed without a fatal diagnostic.
  bool IsValid() const { return valid_; }

  // Returns the extracted workspace path.
  const fs::path &Path() const { return workspace_.Path(); }

private:
  runtime_storage::TemporaryWorkspace workspace_;
  bool valid_ = false;
};

// Parses XML while retaining the established escaped-control compatibility fallback.
bool ParseXmlWithEscapedControlFallback(const fs::path &filePath,
                                        tinyxml2::XMLDocument &document) {
  if (document.LoadFile(filePath.string().c_str()) == tinyxml2::XML_SUCCESS)
    return true;

  std::ifstream input(filePath, std::ios::binary);
  if (!input.is_open())
    return false;
  const std::string raw((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());
  if (raw.empty() || (raw.find("\\n") == std::string::npos &&
                      raw.find("\\r") == std::string::npos &&
                      raw.find("\\t") == std::string::npos))
    return false;

  std::string sanitized;
  sanitized.reserve(raw.size());
  for (std::size_t index = 0; index < raw.size(); ++index) {
    if (raw[index] == '\\' && index + 1 < raw.size()) {
      const char escaped = raw[index + 1];
      if (escaped == 'n' || escaped == 'r' || escaped == 't') {
        sanitized.push_back(escaped == 'n' ? '\n' : escaped == 'r' ? '\r' : '\t');
        ++index;
        continue;
      }
    }
    sanitized.push_back(raw[index]);
  }
  document.Clear();
  return document.Parse(sanitized.c_str(), sanitized.size()) ==
         tinyxml2::XML_SUCCESS;
}

// Reports whether an archive-relative path is portable and safe to publish.
bool IsSafeArchiveRelativePath(const fs::path &path) {
  const std::string value = path.generic_string();
  if (value.empty() || value.front() == '/' || value.find('\\') != std::string::npos)
    return false;
  std::stringstream stream(value);
  std::string component;
  while (std::getline(stream, component, '/')) {
    if (component.empty() || component == "." || component == "..")
      return false;
  }
  return true;
}

// Creates a unique temporary archive path next to the target archive.
fs::path MakeUniqueSiblingArchivePath(const fs::path &targetPath) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::random_device device;
  for (int attempt = 0; attempt < 100; ++attempt) {
    const fs::path candidate =
        targetPath.parent_path() /
        (targetPath.filename().string() + ".tmp." + std::to_string(now) + "." +
         std::to_string(device()) + "." + std::to_string(attempt));
    std::error_code error;
    if (!fs::exists(candidate, error))
      return candidate;
  }
  return targetPath.parent_path() / (targetPath.filename().string() + ".tmp");
}

// Removes a temporary archive without throwing during cleanup.
void RemoveTemporaryArchive(const fs::path &path) {
  if (path.empty())
    return;
  std::error_code ignored;
  fs::remove(path, ignored);
}

// Invokes an optional publication hook before a named stage.
bool AllowPublicationStage(const DocumentMutationPublicationHooks *hooks,
                           const std::string &stage,
                           ArchivePublicationResult &result) {
  if (!hooks || !hooks->beforeStage)
    return true;
  std::string error;
  if (hooks->beforeStage(stage, error))
    return true;
  result.stage = stage;
  result.diagnostic = error.empty()
                          ? "publication stage was rejected by test hook"
                          : error;
  return false;
}

// Writes a directory into a temporary GDTF archive with checked ZIP operations.
ArchivePublicationResult WriteDirectoryArchive(
    const fs::path &sourceDirectory, const fs::path &targetPath,
    const DocumentMutationPublicationHooks *hooks) {
  ArchivePublicationResult result;
  result.tempPath = MakeUniqueSiblingArchivePath(targetPath);
  std::vector<fs::directory_entry> files;
  std::error_code error;
  for (fs::recursive_directory_iterator iterator(sourceDirectory, error), end;
       iterator != end; iterator.increment(error)) {
    if (error) {
      result.stage = "EnumerateArchiveEntries";
      result.diagnostic = error.message();
      return result;
    }
    if (iterator->is_regular_file(error) && !error)
      files.push_back(*iterator);
  }
  std::sort(files.begin(), files.end(),
            [](const fs::directory_entry &left,
               const fs::directory_entry &right) {
              return left.path().generic_string() < right.path().generic_string();
            });

  if (!AllowPublicationStage(hooks, "BeforeOpenTemporaryArchive", result))
    return result;
  wxFileOutputStream output(result.tempPath.string());
  if (!output.IsOk()) {
    result.stage = "OpenTemporaryArchive";
    result.diagnostic = "could not open temporary GDTF archive for writing";
    return result;
  }
  wxZipOutputStream zip(output);
  for (const auto &file : files) {
    const fs::path relative = fs::relative(file.path(), sourceDirectory, error);
    if (error || !IsSafeArchiveRelativePath(relative)) {
      result.stage = "ValidateArchiveEntryPath";
      result.diagnostic = error ? error.message()
                                : "unsafe archive entry path: " +
                                      relative.generic_string();
      return result;
    }
    if (!AllowPublicationStage(hooks, "BeforePutNextEntry", result))
      return result;
    auto *entry = new wxZipEntry(relative.generic_string());
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    if (!zip.PutNextEntry(entry)) {
      result.stage = "PutNextEntry";
      result.diagnostic = "could not create archive entry: " +
                          relative.generic_string();
      return result;
    }
    std::ifstream input(file.path(), std::ios::binary);
    if (!input.is_open()) {
      result.stage = "OpenArchiveInput";
      result.diagnostic = "could not open archive input: " + file.path().string();
      return result;
    }
    char buffer[4096];
    while (input.good()) {
      input.read(buffer, sizeof(buffer));
      const std::streamsize count = input.gcount();
      if (count <= 0)
        continue;
      if (!AllowPublicationStage(hooks, "BeforeWriteEntryBytes", result))
        return result;
      zip.Write(buffer, count);
      if (zip.LastWrite() != static_cast<std::size_t>(count)) {
        result.stage = "WriteEntryBytes";
        result.diagnostic = "could not write archive entry bytes: " +
                            relative.generic_string();
        return result;
      }
    }
    if (!input.eof()) {
      result.stage = "ReadArchiveInput";
      result.diagnostic = "could not read archive input: " + file.path().string();
      return result;
    }
    if (!zip.CloseEntry()) {
      result.stage = "CloseArchiveEntry";
      result.diagnostic = "could not close archive entry: " +
                          relative.generic_string();
      return result;
    }
  }
  if (!AllowPublicationStage(hooks, "BeforeCloseTemporaryArchive", result))
    return result;
  const bool zipClosed = zip.Close();
  const bool outputClosed = output.Close();
  if (!zipClosed || !outputClosed) {
    result.stage = "CloseTemporaryArchive";
    result.diagnostic = "could not close temporary GDTF archive";
    return result;
  }
  result.success = true;
  return result;
}

// Reports whether requested physical-property mutations are finite.
bool ValidateFinitePhysicalPropertyInputs(const DocumentMutationRequest &request,
                                          std::vector<std::string> &errors) {
  bool valid = true;
  if (request.weightSet && !std::isfinite(request.weightKg)) {
    errors.push_back("GDTF mutation rejected non-finite Weight value");
    valid = false;
  }
  if (request.powerSet && !std::isfinite(request.powerW)) {
    errors.push_back("GDTF mutation rejected non-finite PowerConsumption value");
    valid = false;
  }
  return valid;
}

// Replaces the target archive with a completed sibling archive.
bool ReplaceArchiveAtomically(const fs::path &tempPath,
                              const fs::path &targetPath, std::string &error) {
  std::error_code operationError;
#ifdef _WIN32
  const BOOL moved = MoveFileExW(tempPath.wstring().c_str(),
                                 targetPath.wstring().c_str(),
                                 MOVEFILE_REPLACE_EXISTING |
                                     MOVEFILE_WRITE_THROUGH);
  if (!moved)
    operationError = std::error_code(static_cast<int>(GetLastError()),
                                     std::system_category());
#else
  fs::rename(tempPath, targetPath, operationError);
#endif
  if (operationError) {
    error = operationError.message();
    fs::remove(tempPath, operationError);
    return false;
  }
  return true;
}

} // namespace

// Mutates and atomically publishes selected FixtureType document fields.
DocumentMutationResult MutateDocument(
    const std::string &gdtfPath, const DocumentMutationRequest &request,
    const std::string &modifiedByProgram,
    const DocumentMutationPublicationHooks *publicationHooks) {
  DocumentMutationResult result;
  result.publicationPath = gdtfPath;
  if (gdtfPath.empty()) {
    result.errors.push_back("GDTF path is empty");
    return result;
  }
  if (!ValidateFinitePhysicalPropertyInputs(request, result.errors))
    return result;

  MutationWorkspace extraction(gdtfPath);
  if (!extraction.IsValid()) {
    result.errors.push_back("Could not extract GDTF archive");
    return result;
  }
  const fs::path descriptionPath = extraction.Path() / "description.xml";
  tinyxml2::XMLDocument document;
  if (!ParseXmlWithEscapedControlFallback(descriptionPath, document)) {
    result.errors.push_back("Could not parse description.xml");
    return result;
  }
  tinyxml2::XMLElement *fixtureType =
      GdtfMutationAudit::EnsureFixtureType(document);
  if (!fixtureType) {
    result.errors.push_back("description.xml is missing FixtureType");
    return result;
  }

  bool mutated = false;
  if (request.descriptionSet) {
    fixtureType->SetAttribute("Description", request.description.c_str());
    mutated = true;
  }
  if (request.weightSet || request.powerSet) {
    mutated = GdtfMutationAudit::ApplyPhysicalProperties(
                  fixtureType, document,
                  request.weightSet ? std::optional<float>(request.weightKg)
                                    : std::nullopt,
                  request.powerSet ? std::optional<float>(request.powerW)
                                   : std::nullopt) ||
              mutated;
  }
  if (!mutated) {
    result.success = true;
    return result;
  }

  GdtfMutationAudit::AppendRevision(
      fixtureType, document,
      request.revisionText.empty()
          ? "Updated GDTF document fields from Perastage"
          : request.revisionText,
      modifiedByProgram);
  GdtfCanonicalizer::Options canonicalOptions;
  canonicalOptions.allowFixtureTypeIdRepair = true;
  canonicalOptions.stableIdSeed = gdtfPath;
  canonicalOptions.sourceLabel = gdtfPath;
  const GdtfCanonicalizer::Result canonicalResult =
      GdtfCanonicalizer::CanonicalizeDescription(document, canonicalOptions);
  result.changed = canonicalResult.changed || mutated;
  result.warnings = canonicalResult.warnings;
  if (!canonicalResult.success) {
    result.errors = canonicalResult.errors;
    return result;
  }
  if (document.SaveFile(descriptionPath.string().c_str()) !=
      tinyxml2::XML_SUCCESS) {
    result.errors.push_back("Could not save canonical description.xml");
    return result;
  }

  const fs::path targetPath(gdtfPath);
  ArchivePublicationResult publication = WriteDirectoryArchive(
      extraction.Path(), targetPath, publicationHooks);
  if (!publication.success) {
    result.errors.push_back("GDTF publication failed at " + publication.stage +
                            ": " + publication.diagnostic);
    RemoveTemporaryArchive(publication.tempPath);
    return result;
  }
  ArchivePublicationResult hookResult;
  if (!AllowPublicationStage(publicationHooks, "BeforeAtomicReplace",
                             hookResult)) {
    result.errors.push_back("GDTF publication failed at " + hookResult.stage +
                            ": " + hookResult.diagnostic);
    RemoveTemporaryArchive(publication.tempPath);
    return result;
  }
  std::string replaceError;
  if (!ReplaceArchiveAtomically(publication.tempPath, targetPath, replaceError)) {
    result.errors.push_back("GDTF publication failed at AtomicReplace: " +
                            replaceError);
    RemoveTemporaryArchive(publication.tempPath);
    return result;
  }
  result.atomicReplacementCompleted = true;
  result.success = true;
  return result;
}

} // namespace gdtf
