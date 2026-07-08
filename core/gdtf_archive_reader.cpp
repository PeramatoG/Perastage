#include "gdtf_archive_reader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <memory>
#include <system_error>

#include <wx/string.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace gdtf {
namespace {
constexpr std::uint64_t kMaxDescriptionXmlBytes = 64ull * 1024ull * 1024ull;

struct DescriptionCandidate {
  std::string path;
  std::string xml;
};

// Converts ASCII characters to lower case for stable archive-key comparison.
std::string LowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

// Normalizes ZIP entry names to archive-relative paths with forward slashes.
std::string NormalizeArchivePath(const wxString &name) {
  std::string path = name.ToUTF8().data();
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.rfind("./", 0) == 0)
    path.erase(0, 2);
  return path;
}

// Reports whether a normalized archive path is unsafe to materialize or trust.
bool IsUnsafeArchivePath(const std::string &path) {
  if (path.empty() || path.front() == '/' || path.find(':') != std::string::npos)
    return true;
  size_t start = 0;
  while (start <= path.size()) {
    const size_t slash = path.find('/', start);
    const std::string part =
        path.substr(start, slash == std::string::npos ? std::string::npos
                                                      : slash - start);
    if (part == "..")
      return true;
    if (slash == std::string::npos)
      break;
    start = slash + 1;
  }
  return false;
}

// Returns the final archive path component for case-insensitive lookup.
std::string ArchiveFileName(const std::string &path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Adds a structured diagnostic to the archive result.
void AddDiagnostic(ArchiveReadResult &result, ArchiveDiagnosticCode code,
                   std::string message, std::string entryPath = {}) {
  result.diagnostics.push_back({code, std::move(message), std::move(entryPath)});
}

// Reports whether an archive diagnostic prevents a usable read result.
bool IsFatalDiagnostic(ArchiveDiagnosticCode code) {
  switch (code) {
  case ArchiveDiagnosticCode::None:
  case ArchiveDiagnosticCode::NonCanonicalDescriptionXml:
    return false;
  case ArchiveDiagnosticCode::EmptySourcePath:
  case ArchiveDiagnosticCode::OpenFailed:
  case ArchiveDiagnosticCode::NoReadableEntries:
  case ArchiveDiagnosticCode::MissingDescriptionXml:
  case ArchiveDiagnosticCode::DuplicateDescriptionXml:
  case ArchiveDiagnosticCode::AmbiguousDescriptionXml:
  case ArchiveDiagnosticCode::EmptyDescriptionXml:
  case ArchiveDiagnosticCode::UnsafeEntryPath:
  case ArchiveDiagnosticCode::EntryReadFailed:
  case ArchiveDiagnosticCode::EntryTooLarge:
  case ArchiveDiagnosticCode::FilesystemError:
  case ArchiveDiagnosticCode::UnexpectedException:
    return true;
  }
  return true;
}

// Selects description.xml using canonical-first tolerant read rules.
void SelectDescriptionCandidate(ArchiveReadResult &result,
                                const std::vector<DescriptionCandidate> &candidates) {
  std::vector<DescriptionCandidate> exactRoot;
  std::vector<DescriptionCandidate> rootCaseInsensitive;
  std::vector<DescriptionCandidate> nested;
  for (const auto &candidate : candidates) {
    const bool root = candidate.path.find('/') == std::string::npos;
    const std::string lowerName = LowerAscii(ArchiveFileName(candidate.path));
    if (candidate.path == "description.xml")
      exactRoot.push_back(candidate);
    else if (root && lowerName == "description.xml")
      rootCaseInsensitive.push_back(candidate);
    else if (!root && lowerName == "description.xml")
      nested.push_back(candidate);
  }

  if (exactRoot.size() > 1) {
    AddDiagnostic(result, ArchiveDiagnosticCode::DuplicateDescriptionXml,
                  "The GDTF archive contains multiple canonical description.xml entries.",
                  exactRoot.front().path);
    return;
  }
  if (!exactRoot.empty()) {
    result.descriptionXml = exactRoot.front().xml;
    result.descriptionEntryPath = exactRoot.front().path;
    result.standardsCompliantDescriptionLocation = true;
    return;
  }

  if (rootCaseInsensitive.size() == 1) {
    result.descriptionXml = rootCaseInsensitive.front().xml;
    result.descriptionEntryPath = rootCaseInsensitive.front().path;
    result.usedCompatibilityDescriptionFallback = true;
    AddDiagnostic(result, ArchiveDiagnosticCode::NonCanonicalDescriptionXml,
                  "Using non-standard root description.xml name for compatibility.",
                  rootCaseInsensitive.front().path);
    return;
  }
  if (rootCaseInsensitive.size() > 1) {
    AddDiagnostic(result, ArchiveDiagnosticCode::DuplicateDescriptionXml,
                  "The GDTF archive contains multiple root case-insensitive description.xml entries.",
                  rootCaseInsensitive.front().path);
    return;
  }

  if (nested.size() == 1) {
    result.descriptionXml = nested.front().xml;
    result.descriptionEntryPath = nested.front().path;
    result.usedCompatibilityDescriptionFallback = true;
    AddDiagnostic(result, ArchiveDiagnosticCode::NonCanonicalDescriptionXml,
                  "Using nested description.xml for compatibility; canonical GDTF stores it at the archive root.",
                  nested.front().path);
    return;
  }
  if (nested.size() > 1) {
    AddDiagnostic(result, ArchiveDiagnosticCode::AmbiguousDescriptionXml,
                  "The GDTF archive contains multiple nested description.xml candidates.",
                  nested.front().path);
    return;
  }

  AddDiagnostic(result, ArchiveDiagnosticCode::MissingDescriptionXml,
                "The GDTF archive does not contain description.xml.");
}

// Reads the current ZIP entry with a bounded byte limit.
bool ReadCurrentEntry(wxZipInputStream &zipInput, std::string &out,
                      std::uint64_t maxBytes) {
  out.clear();
  char buffer[4096];
  std::uint64_t total = 0;
  while (true) {
    zipInput.Read(buffer, sizeof(buffer));
    const size_t count = zipInput.LastRead();
    if (count == 0)
      break;
    total += static_cast<std::uint64_t>(count);
    if (total > maxBytes)
      return false;
    out.append(buffer, buffer + count);
  }
  return zipInput.GetLastError() == wxSTREAM_NO_ERROR ||
         zipInput.GetLastError() == wxSTREAM_EOF;
}
} // namespace

// Reports whether the archive read found one usable description.xml payload.
bool ArchiveReadResult::Success() const {
  if (descriptionXml.empty())
    return false;
  return std::none_of(diagnostics.begin(), diagnostics.end(),
                      [](const ArchiveDiagnostic &diagnostic) {
                        return IsFatalDiagnostic(diagnostic.code);
                      });
}

// Opens a GDTF archive, inventories entries, and reads description.xml only.
ArchiveReadResult ReadGdtfArchive(const std::filesystem::path &sourcePath) {
  ArchiveReadResult result;
  result.sourcePath = sourcePath;
  try {
    if (sourcePath.empty()) {
      AddDiagnostic(result, ArchiveDiagnosticCode::EmptySourcePath,
                    "GDTF source path is empty.");
      return result;
    }

    std::error_code ec;
    if (!std::filesystem::exists(sourcePath, ec) || ec) {
      AddDiagnostic(result, ArchiveDiagnosticCode::OpenFailed,
                    "Could not access GDTF archive.");
      return result;
    }
    if (!std::filesystem::is_regular_file(sourcePath, ec) || ec) {
      AddDiagnostic(result, ArchiveDiagnosticCode::OpenFailed,
                    "GDTF source is not a regular file.");
      return result;
    }

    wxFileInputStream input(
        wxString::FromUTF8(sourcePath.generic_string().c_str()));
    if (!input.IsOk()) {
      AddDiagnostic(result, ArchiveDiagnosticCode::OpenFailed,
                    "Could not open GDTF archive.");
      return result;
    }

    wxZipInputStream zipInput(input);
    std::unique_ptr<wxZipEntry> entry;
    std::vector<DescriptionCandidate> descriptionCandidates;
    while ((entry.reset(zipInput.GetNextEntry())), entry) {
      const std::string entryPath = NormalizeArchivePath(entry->GetName());
      ArchiveEntry inventoryEntry;
      inventoryEntry.path = entryPath;
      inventoryEntry.directory = entry->IsDir();
      const wxFileOffset size = entry->GetSize();
      if (size >= 0) {
        inventoryEntry.sizeKnown = true;
        inventoryEntry.size = static_cast<std::uint64_t>(size);
      }
      result.entries.push_back(inventoryEntry);

      if (IsUnsafeArchivePath(entryPath)) {
        AddDiagnostic(result, ArchiveDiagnosticCode::UnsafeEntryPath,
                      "The GDTF archive contains an unsafe entry path.",
                      entryPath);
        continue;
      }

      if (!entry->IsDir() &&
          LowerAscii(ArchiveFileName(entryPath)) == "description.xml") {
        if (inventoryEntry.sizeKnown &&
            inventoryEntry.size > kMaxDescriptionXmlBytes) {
          AddDiagnostic(result, ArchiveDiagnosticCode::EntryTooLarge,
                        "description.xml is larger than the safe read limit.",
                        entryPath);
          continue;
        }
        std::string currentXml;
        if (!ReadCurrentEntry(zipInput, currentXml, kMaxDescriptionXmlBytes)) {
          AddDiagnostic(result, ArchiveDiagnosticCode::EntryReadFailed,
                        "Could not read description.xml from the GDTF archive.",
                        entryPath);
          continue;
        }
        descriptionCandidates.push_back({entryPath, std::move(currentXml)});
      }
    }

    if (result.entries.empty()) {
      AddDiagnostic(result, ArchiveDiagnosticCode::NoReadableEntries,
                    "The GDTF archive does not contain readable entries.");
    }
    SelectDescriptionCandidate(result, descriptionCandidates);
    if (result.descriptionEntryPath.empty()) {
      return result;
    }
    if (result.descriptionXml.empty() ||
        result.descriptionXml.find_first_not_of(" \t\r\n") == std::string::npos) {
      AddDiagnostic(result, ArchiveDiagnosticCode::EmptyDescriptionXml,
                    "description.xml is empty.", result.descriptionEntryPath);
      result.descriptionXml.clear();
    }
  } catch (const std::filesystem::filesystem_error &error) {
    AddDiagnostic(result, ArchiveDiagnosticCode::FilesystemError,
                  std::string("Filesystem error while reading GDTF archive: ") +
                      error.what());
  } catch (const std::system_error &error) {
    AddDiagnostic(result, ArchiveDiagnosticCode::FilesystemError,
                  std::string("System error while reading GDTF archive: ") +
                      error.what());
  } catch (const std::exception &error) {
    AddDiagnostic(result, ArchiveDiagnosticCode::UnexpectedException,
                  std::string("Unexpected error while reading GDTF archive: ") +
                      error.what());
  } catch (...) {
    AddDiagnostic(result, ArchiveDiagnosticCode::UnexpectedException,
                  "Unknown error while reading GDTF archive.");
  }

  return result;
}

} // namespace gdtf
