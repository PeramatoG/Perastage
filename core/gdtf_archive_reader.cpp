#include "gdtf_archive_reader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>

#include <wx/string.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace gdtf {
namespace {
constexpr std::uint64_t kMaxDescriptionXmlBytes = 64ull * 1024ull * 1024ull;

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
  while (!path.empty() && path.front() == '/')
    path.erase(path.begin());
  return path;
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
  return !descriptionXml.empty() && diagnostics.empty();
}

// Opens a GDTF archive, inventories entries, and reads description.xml only.
ArchiveReadResult ReadGdtfArchive(const std::filesystem::path &sourcePath) {
  ArchiveReadResult result;
  result.sourcePath = sourcePath;
  if (sourcePath.empty()) {
    AddDiagnostic(result, ArchiveDiagnosticCode::EmptySourcePath,
                  "GDTF source path is empty.");
    return result;
  }

  wxFileInputStream input(wxString::FromUTF8(sourcePath.generic_string().c_str()));
  if (!input.IsOk()) {
    AddDiagnostic(result, ArchiveDiagnosticCode::OpenFailed,
                  "Could not open GDTF archive.");
    return result;
  }

  wxZipInputStream zipInput(input);
  std::unique_ptr<wxZipEntry> entry;
  std::vector<std::string> descriptionCandidates;
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

    if (!entry->IsDir() && LowerAscii(ArchiveFileName(entryPath)) ==
                               "description.xml") {
      descriptionCandidates.push_back(entryPath);
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
      if (result.descriptionXml.empty()) {
        result.descriptionXml = std::move(currentXml);
        result.descriptionEntryPath = entryPath;
      }
    }
  }

  if (result.entries.empty()) {
    AddDiagnostic(result, ArchiveDiagnosticCode::NoReadableEntries,
                  "The GDTF archive does not contain readable entries.");
  }
  if (descriptionCandidates.empty()) {
    AddDiagnostic(result, ArchiveDiagnosticCode::MissingDescriptionXml,
                  "The GDTF archive does not contain description.xml.");
  } else if (descriptionCandidates.size() > 1) {
    std::sort(descriptionCandidates.begin(), descriptionCandidates.end());
    AddDiagnostic(result, ArchiveDiagnosticCode::DuplicateDescriptionXml,
                  "The GDTF archive contains multiple case-insensitive description.xml entries.",
                  descriptionCandidates.front());
    result.descriptionXml.clear();
    result.descriptionEntryPath.clear();
  }

  return result;
}

} // namespace gdtf
