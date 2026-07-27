#include "gdtf_archive_reader.h"

#include "wx_path_utils.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
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
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

struct RawZipEntryName {
  std::string bytes;
  bool utf8Flag = false;
};

struct DecodedZipEntryName {
  std::string path;
  bool usedUtf8Fallback = false;
  bool failed = false;
};

// Reports whether raw bytes contain only portable ASCII characters.
bool IsAscii(const std::string &value) {
  return std::all_of(value.begin(), value.end(),
                     [](unsigned char c) { return c < 0x80; });
}

// Reports whether raw bytes are a well-formed UTF-8 sequence.
bool IsValidUtf8(const std::string &value) {
  size_t i = 0;
  while (i < value.size()) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    size_t extra = 0;
    uint32_t code = 0;
    if (c <= 0x7F) {
      ++i;
      continue;
    }
    if ((c & 0xE0) == 0xC0) {
      extra = 1;
      code = c & 0x1F;
      if (code == 0)
        return false;
    } else if ((c & 0xF0) == 0xE0) {
      extra = 2;
      code = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
      extra = 3;
      code = c & 0x07;
    } else
      return false;
    if (i + extra >= value.size())
      return false;
    for (size_t j = 1; j <= extra; ++j) {
      const unsigned char t = static_cast<unsigned char>(value[i + j]);
      if ((t & 0xC0) != 0x80)
        return false;
      code = (code << 6) | (t & 0x3F);
    }
    if ((extra == 1 && code < 0x80) || (extra == 2 && code < 0x800) ||
        (extra == 3 && code < 0x10000) || code > 0x10FFFF ||
        (code >= 0xD800 && code <= 0xDFFF))
      return false;
    i += extra + 1;
  }
  return true;
}

// Reads a little-endian 16-bit value from ZIP metadata.
uint16_t ReadLe16(const std::vector<unsigned char> &data, size_t offset) {
  return static_cast<uint16_t>(data[offset]) |
         (static_cast<uint16_t>(data[offset + 1]) << 8);
}

// Reads a little-endian 32-bit value from ZIP metadata.
uint32_t ReadLe32(const std::vector<unsigned char> &data, size_t offset) {
  return static_cast<uint32_t>(data[offset]) |
         (static_cast<uint32_t>(data[offset + 1]) << 8) |
         (static_cast<uint32_t>(data[offset + 2]) << 16) |
         (static_cast<uint32_t>(data[offset + 3]) << 24);
}

// Locates the ZIP end-of-central-directory record in bounded trailing data.
std::optional<size_t>
FindEndOfCentralDirectory(const std::vector<unsigned char> &data) {
  if (data.size() < 22)
    return std::nullopt;
  const size_t maxComment = 0xffff;
  const size_t minOffset =
      data.size() > 22 + maxComment ? data.size() - 22 - maxComment : 0;
  for (size_t i = data.size() - 22;; --i) {
    if (ReadLe32(data, i) == 0x06054b50)
      return i;
    if (i == minOffset)
      break;
  }
  return std::nullopt;
}

// Reads raw ZIP central-directory entry names before wxWidgets decodes them.
std::vector<RawZipEntryName>
ReadRawCentralDirectoryNames(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return {};
  std::vector<unsigned char> data((std::istreambuf_iterator<char>(input)), {});
  const std::optional<size_t> eocdOffset = FindEndOfCentralDirectory(data);
  if (!eocdOffset)
    return {};

  const uint16_t entryCount = ReadLe16(data, *eocdOffset + 10);
  size_t i = ReadLe32(data, *eocdOffset + 16);
  std::vector<RawZipEntryName> names;
  for (uint16_t entryIndex = 0;
       entryIndex < entryCount && i + 46 <= data.size(); ++entryIndex) {
    if (ReadLe32(data, i) != 0x02014b50)
      break;
    const uint16_t flags = ReadLe16(data, i + 8);
    const uint16_t nameLen = ReadLe16(data, i + 28);
    const uint16_t extraLen = ReadLe16(data, i + 30);
    const uint16_t commentLen = ReadLe16(data, i + 32);
    if (i + 46u + nameLen + extraLen + commentLen > data.size())
      break;
    names.push_back(
        {std::string(reinterpret_cast<const char *>(&data[i + 46]), nameLen),
         (flags & (1u << 11)) != 0});
    i += 46u + nameLen + extraLen + commentLen;
  }
  return names;
}

// Decodes one raw ZIP entry name according to GDTF compatibility policy.
DecodedZipEntryName DecodeZipEntryName(const RawZipEntryName &raw) {
  DecodedZipEntryName decoded;
  if (raw.utf8Flag || !IsAscii(raw.bytes)) {
    if (!IsValidUtf8(raw.bytes)) {
      decoded.failed = true;
      return decoded;
    }
    decoded.path = raw.bytes;
    decoded.usedUtf8Fallback = !raw.utf8Flag && !IsAscii(raw.bytes);
    return decoded;
  }
  decoded.path = raw.bytes;
  return decoded;
}

// Normalizes ZIP entry names to archive-relative paths with forward slashes.
std::string NormalizeArchivePath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.rfind("./", 0) == 0)
    path.erase(0, 2);
  return path;
}

// Reports whether a normalized archive path is unsafe to materialize or trust.
bool IsUnsafeArchivePath(const std::string &path) {
  if (path.empty() || path.front() == '/' ||
      path.find(':') != std::string::npos)
    return true;
  size_t start = 0;
  while (start <= path.size()) {
    const size_t slash = path.find('/', start);
    const std::string part = path.substr(
        start, slash == std::string::npos ? std::string::npos : slash - start);
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


// Converts UTF-8 archive-style text to a filesystem path component.
std::filesystem::path PathFromUtf8(const std::string &path) {
  std::u8string utf8(path.begin(), path.end());
  return std::filesystem::path(utf8);
}

// Builds preferred wheel resource paths for standard and compatible lookup.
std::vector<std::string> BuildResourcePreferredPaths(const std::string &normalizedRequest) {
  std::vector<std::string> preferred;
  auto addPath = [&](const std::string &path) {
    const std::string normalized = NormalizeArchivePath(path);
    if (std::find(preferred.begin(), preferred.end(), normalized) == preferred.end())
      preferred.push_back(normalized);
  };
  addPath(normalizedRequest);
  const bool hasDirectory = normalizedRequest.find('/') != std::string::npos;
  const bool hasExtension = ArchiveFileName(normalizedRequest).find('.') != std::string::npos;
  static const char *extensions[] = {".png", ".svg", ".jpg", ".jpeg", ".bmp"};
  static const char *directories[] = {"wheels/", "wheels/gobos/", "wheels/animation/",
                                      "wheels/graphic/", "graphics/"};
  if (!hasDirectory) {
    for (const char *directory : directories) {
      addPath(std::string(directory) + normalizedRequest);
      if (!hasExtension) {
        for (const char *extension : extensions)
          addPath(std::string(directory) + normalizedRequest + extension);
      }
    }
  } else if (!hasExtension) {
    for (const char *extension : extensions)
      addPath(normalizedRequest + extension);
  }
  return preferred;
}

// Reads a bounded filesystem resource from an already-exploded GDTF resource root.
bool TryReadExplodedGdtfResource(const std::filesystem::path &sourcePath,
                                 const std::string &normalizedRequest,
                                 const std::vector<std::string> &preferredPaths,
                                 const std::vector<std::filesystem::path> &extraResourceRoots,
                                 std::uint64_t maxBytes,
                                 GdtfResourceReadResult &result) {
  std::vector<std::filesystem::path> roots;
  for (const auto &root : extraResourceRoots) {
    if (!root.empty())
      roots.push_back(root);
  }
  std::error_code ec;
  if (std::filesystem::is_directory(sourcePath, ec) && !ec)
    roots.push_back(sourcePath);
  const std::filesystem::path parent = sourcePath.parent_path();
  if (!parent.empty()) {
    roots.push_back(parent / sourcePath.stem());
    roots.push_back(parent);
  }

  struct FilesystemCandidate { std::filesystem::path root; std::string relative; };
  std::vector<FilesystemCandidate> candidates;
  auto addCandidate = [&](const std::filesystem::path &root, const std::string &relative) {
    if (root.empty() || relative.empty() || IsUnsafeArchivePath(relative))
      return;
    std::error_code rootEc;
    const auto canonicalRoot = std::filesystem::weakly_canonical(root, rootEc);
    if (rootEc)
      return;
    const std::filesystem::path candidatePath = root / PathFromUtf8(relative);
    std::error_code fileEc;
    if (!std::filesystem::is_regular_file(candidatePath, fileEc) || fileEc)
      return;
    const auto canonicalCandidate = std::filesystem::weakly_canonical(candidatePath, fileEc);
    if (fileEc)
      return;
    const auto rootText = canonicalRoot.native();
    const auto candidateText = canonicalCandidate.native();
    if (candidateText.size() < rootText.size() ||
        !std::equal(rootText.begin(), rootText.end(), candidateText.begin()))
      return;
    const std::string normalizedRelative = NormalizeArchivePath(relative);
    auto exists = std::find_if(candidates.begin(), candidates.end(), [&](const FilesystemCandidate &candidate) {
      return LowerAscii(candidate.relative) == LowerAscii(normalizedRelative);
    });
    if (exists == candidates.end())
      candidates.push_back({root, normalizedRelative});
  };

  for (const auto &root : roots) {
    for (const auto &relative : preferredPaths)
      addCandidate(root, relative);
    addCandidate(root, normalizedRequest);
  }

  if (candidates.empty())
    return false;
  if (candidates.size() > 1) {
    result.diagnostics.push_back({ArchiveDiagnosticCode::ResourcePathAmbiguous,
                                  "Requested GDTF resource path is ambiguous in extracted resource folders.",
                                  normalizedRequest});
    return true;
  }

  const auto &candidate = candidates.front();
  const std::filesystem::path filePath = candidate.root / PathFromUtf8(candidate.relative);
  std::error_code sizeEc;
  const auto fileSize = std::filesystem::file_size(filePath, sizeEc);
  if (sizeEc || fileSize > maxBytes) {
    result.diagnostics.push_back({sizeEc ? ArchiveDiagnosticCode::ResourceReadFailed
                                         : ArchiveDiagnosticCode::ResourceEntryTooLarge,
                                  sizeEc ? "Could not read extracted GDTF resource size."
                                         : "Requested extracted GDTF resource exceeds the safe read limit.",
                                  candidate.relative});
    return true;
  }

  std::ifstream input(filePath, std::ios::binary);
  if (!input) {
    result.diagnostics.push_back({ArchiveDiagnosticCode::ResourceReadFailed,
                                  "Could not open extracted GDTF resource.", candidate.relative});
    return true;
  }
  std::vector<unsigned char> bytes(static_cast<std::size_t>(fileSize));
  if (!bytes.empty())
    input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!input && !input.eof()) {
    result.diagnostics.push_back({ArchiveDiagnosticCode::ResourceReadFailed,
                                  "Could not read extracted GDTF resource.", candidate.relative});
    return true;
  }

  result.entryPath = candidate.relative;
  result.bytes = std::move(bytes);
  result.size = static_cast<std::uint64_t>(result.bytes.size());
  result.mediaKind = LowerAscii(ArchiveFileName(result.entryPath));
  result.caseInsensitiveFallback = true;
  result.filesystemFallback = true;
  result.diagnostics.push_back({ArchiveDiagnosticCode::Utf8FallbackUsed,
                                "Using extracted GDTF resource folder fallback.", result.entryPath});
  return true;
}

// Adds a structured diagnostic to the archive result.
void AddDiagnostic(ArchiveReadResult &result, ArchiveDiagnosticCode code,
                   std::string message, std::string entryPath = {}) {
  result.diagnostics.push_back(
      {code, std::move(message), std::move(entryPath)});
}

// Rejects malformed raw identities before wxWidgets can skip or reorder them.
bool ValidateRawZipEntryNames(const std::vector<RawZipEntryName> &rawNames,
                              ArchiveReadResult &result) {
  bool valid = true;
  for (const RawZipEntryName &rawName : rawNames) {
    if (!DecodeZipEntryName(rawName).failed)
      continue;
    AddDiagnostic(
        result, ArchiveDiagnosticCode::FilenameDecodeFailed,
        "A GDTF archive entry filename could not be decoded safely.");
    valid = false;
  }
  return valid;
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
  case ArchiveDiagnosticCode::FilenameDecodeFailed:
  case ArchiveDiagnosticCode::FilenameEncodingAmbiguous:
  case ArchiveDiagnosticCode::ResourceNotFound:
  case ArchiveDiagnosticCode::ResourcePathAmbiguous:
  case ArchiveDiagnosticCode::ResourceEntryTooLarge:
  case ArchiveDiagnosticCode::ResourceReadFailed:
  case ArchiveDiagnosticCode::UnsafeResourcePath:
  case ArchiveDiagnosticCode::ResourceFilenameDecodeFailed:
    return true;
  case ArchiveDiagnosticCode::Utf8FlagMissing:
  case ArchiveDiagnosticCode::Utf8FallbackUsed:
  case ArchiveDiagnosticCode::LegacyFilenameEncodingUsed:
    return false;
  }
  return true;
}

// Selects description.xml using canonical-first tolerant read rules.
void SelectDescriptionCandidate(
    ArchiveReadResult &result,
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
    AddDiagnostic(
        result, ArchiveDiagnosticCode::DuplicateDescriptionXml,
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
    AddDiagnostic(
        result, ArchiveDiagnosticCode::NonCanonicalDescriptionXml,
        "Using non-standard root description.xml name for compatibility.",
        rootCaseInsensitive.front().path);
    return;
  }
  if (rootCaseInsensitive.size() > 1) {
    AddDiagnostic(result, ArchiveDiagnosticCode::DuplicateDescriptionXml,
                  "The GDTF archive contains multiple root case-insensitive "
                  "description.xml entries.",
                  rootCaseInsensitive.front().path);
    return;
  }

  if (nested.size() == 1) {
    result.descriptionXml = nested.front().xml;
    result.descriptionEntryPath = nested.front().path;
    result.usedCompatibilityDescriptionFallback = true;
    AddDiagnostic(result, ArchiveDiagnosticCode::NonCanonicalDescriptionXml,
                  "Using nested description.xml for compatibility; canonical "
                  "GDTF stores it at the archive root.",
                  nested.front().path);
    return;
  }
  if (nested.size() > 1) {
    AddDiagnostic(
        result, ArchiveDiagnosticCode::AmbiguousDescriptionXml,
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

// Copies the current ZIP entry to disk with a bounded write loop.
bool WriteCurrentEntry(wxZipInputStream &zipInput,
                       const std::filesystem::path &outPath) {
  std::ofstream output(outPath, std::ios::binary);
  if (!output.is_open())
    return false;

  char buffer[4096];
  while (true) {
    zipInput.Read(buffer, sizeof(buffer));
    const size_t count = zipInput.LastRead();
    if (count == 0)
      break;
    output.write(buffer, static_cast<std::streamsize>(count));
    if (!output.good())
      return false;
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
        WxPathUtils::WxStringFromFilesystemPath(sourcePath));
    if (!input.IsOk()) {
      AddDiagnostic(result, ArchiveDiagnosticCode::OpenFailed,
                    "Could not open GDTF archive.");
      return result;
    }

    const std::vector<RawZipEntryName> rawNames =
        ReadRawCentralDirectoryNames(sourcePath);
    if (!ValidateRawZipEntryNames(rawNames, result))
      return result;
    wxZipInputStream zipInput(input);
    std::unique_ptr<wxZipEntry> entry;
    size_t entryIndex = 0;
    std::vector<DescriptionCandidate> descriptionCandidates;
    while ((entry.reset(zipInput.GetNextEntry())), entry) {
      DecodedZipEntryName decodedName;
      if (entryIndex < rawNames.size()) {
        decodedName = DecodeZipEntryName(rawNames[entryIndex]);
      } else {
        const wxScopedCharBuffer utf8 = entry->GetName().ToUTF8();
        decodedName.path = utf8 ? std::string(utf8.data()) : std::string();
      }
      ++entryIndex;
      std::string entryPath = NormalizeArchivePath(decodedName.path);
      if (decodedName.failed) {
        AddDiagnostic(
            result, ArchiveDiagnosticCode::FilenameDecodeFailed,
            "A GDTF archive entry filename could not be decoded safely.");
        continue;
      }
      ArchiveEntry inventoryEntry;
      inventoryEntry.path = entryPath;
      inventoryEntry.directory = entry->IsDir();
      inventoryEntry.nameUsedUtf8CompatibilityFallback =
          decodedName.usedUtf8Fallback;
      if (decodedName.usedUtf8Fallback)
        ++result.utf8FlagMissingEntryCount;
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

    if (result.utf8FlagMissingEntryCount > 0) {
      AddDiagnostic(result, ArchiveDiagnosticCode::Utf8FallbackUsed,
                    "GDTF archive uses valid UTF-8 filenames without the ZIP "
                    "UTF-8 flag; compatibility fallback applied to " +
                        std::to_string(result.utf8FlagMissingEntryCount) +
                        " entries.");
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
        result.descriptionXml.find_first_not_of(" \t\r\n") ==
            std::string::npos) {
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

// Reports whether a resource read found one usable bounded entry payload.
bool GdtfResourceReadResult::Success() const {
  return !entryPath.empty() && !bytes.empty() &&
         std::none_of(diagnostics.begin(), diagnostics.end(),
                      [](const ArchiveDiagnostic &diagnostic) {
                        return IsFatalDiagnostic(diagnostic.code);
                      });
}

// Reads one requested GDTF archive resource without extracting the archive.
GdtfResourceReadResult ReadGdtfArchiveResource(
    const std::filesystem::path &sourcePath,
    const std::string &requestedPath,
    std::uint64_t maxBytes,
    const std::vector<std::filesystem::path> &extraResourceRoots) {
  GdtfResourceReadResult result;
  result.sourcePath = sourcePath;
  result.requestedPath = requestedPath;
  const std::string normalizedRequest = NormalizeArchivePath(requestedPath);
  if (sourcePath.empty() || normalizedRequest.empty()) {
    result.diagnostics.push_back({ArchiveDiagnosticCode::EmptySourcePath,
                                  "GDTF source path or resource path is empty.", normalizedRequest});
    return result;
  }
  if (IsUnsafeArchivePath(normalizedRequest)) {
    result.diagnostics.push_back({ArchiveDiagnosticCode::UnsafeResourcePath,
                                  "Requested GDTF resource path is unsafe.", normalizedRequest});
    return result;
  }
  const std::vector<std::string> preferredPaths = BuildResourcePreferredPaths(normalizedRequest);
  const std::vector<RawZipEntryName> rawNames =
      ReadRawCentralDirectoryNames(sourcePath);
  for (const RawZipEntryName &rawName : rawNames) {
    if (!DecodeZipEntryName(rawName).failed)
      continue;
    result.diagnostics.push_back(
        {ArchiveDiagnosticCode::FilenameDecodeFailed,
         "A GDTF archive entry filename could not be decoded safely.", {}});
    return result;
  }
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(sourcePath));
  if (!input.IsOk()) {
    if (TryReadExplodedGdtfResource(sourcePath, normalizedRequest, preferredPaths, extraResourceRoots, maxBytes, result))
      return result;
    result.diagnostics.push_back({ArchiveDiagnosticCode::OpenFailed,
                                  "Could not open GDTF archive.", normalizedRequest});
    return result;
  }

  const std::string lowerRequest = LowerAscii(normalizedRequest);
  const std::string requestFile = LowerAscii(ArchiveFileName(normalizedRequest));
  std::vector<std::string> lowerPreferredPaths;
  lowerPreferredPaths.reserve(preferredPaths.size());
  for (const auto &path : preferredPaths)
    lowerPreferredPaths.push_back(LowerAscii(path));
  struct Candidate { std::string path; bool exact = false; bool preferred = false; };
  std::vector<Candidate> candidates;
  wxZipInputStream inventory(input);
  std::unique_ptr<wxZipEntry> entry;
  size_t entryIndex = 0;
  while ((entry.reset(inventory.GetNextEntry())), entry) {
    DecodedZipEntryName decodedName;
    if (entryIndex < rawNames.size())
      decodedName = DecodeZipEntryName(rawNames[entryIndex]);
    else {
      const wxScopedCharBuffer utf8 = entry->GetName().ToUTF8();
      decodedName.path = utf8 ? std::string(utf8.data()) : std::string();
    }
    ++entryIndex;
    const std::string path = NormalizeArchivePath(decodedName.path);
    if (entry->IsDir() || IsUnsafeArchivePath(path))
      continue;
    const std::string lowerPath = LowerAscii(path);
    const std::string lowerFile = LowerAscii(ArchiveFileName(path));
    const bool preferred = std::find(lowerPreferredPaths.begin(), lowerPreferredPaths.end(), lowerPath) != lowerPreferredPaths.end();
    if (path == normalizedRequest)
      candidates.push_back({path, true, preferred});
    else if (preferred || lowerPath == lowerRequest || lowerFile == requestFile ||
             (!requestFile.empty() && lowerFile.rfind(requestFile + ".", 0) == 0))
      candidates.push_back({path, false, preferred});
  }
  auto exact = std::find_if(candidates.begin(), candidates.end(), [](const Candidate &candidate) {
    return candidate.exact;
  });
  if (exact != candidates.end()) {
    result.entryPath = exact->path;
  } else {
    std::vector<Candidate> preferredCandidates;
    for (const auto &candidate : candidates) {
      if (candidate.preferred)
        preferredCandidates.push_back(candidate);
    }
    if (preferredCandidates.size() == 1) {
      result.entryPath = preferredCandidates.front().path;
      result.caseInsensitiveFallback = true;
      result.diagnostics.push_back({ArchiveDiagnosticCode::Utf8FallbackUsed,
                                    "Using canonical wheel resource path fallback.", result.entryPath});
    } else if (candidates.size() == 1) {
      result.entryPath = candidates.front().path;
      result.caseInsensitiveFallback = true;
      result.diagnostics.push_back({ArchiveDiagnosticCode::Utf8FallbackUsed,
                                    "Using unambiguous compatible resource path fallback.", result.entryPath});
    } else if (candidates.empty()) {
      if (TryReadExplodedGdtfResource(sourcePath, normalizedRequest, preferredPaths, extraResourceRoots, maxBytes, result))
        return result;
      result.diagnostics.push_back({ArchiveDiagnosticCode::ResourceNotFound,
                                    "Requested GDTF resource is missing.", normalizedRequest});
      return result;
    } else {
      result.diagnostics.push_back({ArchiveDiagnosticCode::ResourcePathAmbiguous,
                                    "Requested GDTF resource path is ambiguous.", normalizedRequest});
      return result;
    }
  }

  wxFileInputStream dataInput(
      WxPathUtils::WxStringFromFilesystemPath(sourcePath));
  wxZipInputStream zipInput(dataInput);
  entryIndex = 0;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    DecodedZipEntryName decodedName;
    if (entryIndex < rawNames.size())
      decodedName = DecodeZipEntryName(rawNames[entryIndex]);
    else {
      const wxScopedCharBuffer utf8 = entry->GetName().ToUTF8();
      decodedName.path = utf8 ? std::string(utf8.data()) : std::string();
    }
    ++entryIndex;
    const std::string path = NormalizeArchivePath(decodedName.path);
    if (path != result.entryPath)
      continue;
    const wxFileOffset knownSize = entry->GetSize();
    if (knownSize >= 0 && static_cast<std::uint64_t>(knownSize) > maxBytes) {
      result.diagnostics.push_back({ArchiveDiagnosticCode::ResourceEntryTooLarge,
                                    "Requested GDTF resource exceeds the safe read limit.", result.entryPath});
      return result;
    }
    std::string bytes;
    if (!ReadCurrentEntry(zipInput, bytes, maxBytes)) {
      result.diagnostics.push_back({ArchiveDiagnosticCode::ResourceReadFailed,
                                    "Could not read requested GDTF resource.", result.entryPath});
      return result;
    }
    result.bytes.assign(bytes.begin(), bytes.end());
    result.size = static_cast<std::uint64_t>(result.bytes.size());
    result.mediaKind = LowerAscii(ArchiveFileName(result.entryPath));
    return result;
  }
  result.diagnostics.push_back({ArchiveDiagnosticCode::ResourceReadFailed,
                                "Requested GDTF resource disappeared during read.", result.entryPath});
  return result;
}

// Extracts a GDTF archive using the shared Unicode-safe entry-name policy.
ArchiveReadResult
ExtractGdtfArchive(const std::filesystem::path &sourcePath,
                   const std::filesystem::path &destinationRoot) {
  ArchiveReadResult result;
  result.sourcePath = sourcePath;
  try {
    if (sourcePath.empty() || destinationRoot.empty()) {
      AddDiagnostic(result, ArchiveDiagnosticCode::EmptySourcePath,
                    "GDTF source or destination path is empty.");
      return result;
    }

    std::error_code ec;
    std::filesystem::create_directories(destinationRoot, ec);
    if (ec) {
      AddDiagnostic(result, ArchiveDiagnosticCode::FilesystemError,
                    "Could not create GDTF extraction directory.");
      return result;
    }

    wxFileInputStream input(
        WxPathUtils::WxStringFromFilesystemPath(sourcePath));
    if (!input.IsOk()) {
      AddDiagnostic(result, ArchiveDiagnosticCode::OpenFailed,
                    "Could not open GDTF archive.");
      return result;
    }

    const std::vector<RawZipEntryName> rawNames =
        ReadRawCentralDirectoryNames(sourcePath);
    if (!ValidateRawZipEntryNames(rawNames, result))
      return result;
    wxZipInputStream zipInput(input);
    std::unique_ptr<wxZipEntry> entry;
    size_t entryIndex = 0;
    while ((entry.reset(zipInput.GetNextEntry())), entry) {
      DecodedZipEntryName decodedName;
      if (entryIndex < rawNames.size()) {
        decodedName = DecodeZipEntryName(rawNames[entryIndex]);
      } else {
        const wxScopedCharBuffer utf8 = entry->GetName().ToUTF8();
        decodedName.path = utf8 ? std::string(utf8.data()) : std::string();
      }
      ++entryIndex;

      const std::string entryPath = NormalizeArchivePath(decodedName.path);
      if (decodedName.failed) {
        AddDiagnostic(
            result, ArchiveDiagnosticCode::FilenameDecodeFailed,
            "A GDTF archive entry filename could not be decoded safely.");
        continue;
      }

      ArchiveEntry inventoryEntry;
      inventoryEntry.path = entryPath;
      inventoryEntry.directory = entry->IsDir();
      inventoryEntry.nameUsedUtf8CompatibilityFallback =
          decodedName.usedUtf8Fallback;
      if (decodedName.usedUtf8Fallback)
        ++result.utf8FlagMissingEntryCount;
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

      std::u8string relativeUtf8;
      relativeUtf8.reserve(entryPath.size());
      for (char ch : entryPath)
        relativeUtf8.push_back(static_cast<char8_t>(ch));
      const std::filesystem::path relative =
          std::filesystem::path(std::move(relativeUtf8));
      const std::filesystem::path destinationPath = destinationRoot / relative;
      const std::filesystem::path normalizedRoot =
          std::filesystem::weakly_canonical(destinationRoot, ec);
      if (ec) {
        AddDiagnostic(result, ArchiveDiagnosticCode::FilesystemError,
                      "Could not resolve GDTF extraction directory.",
                      entryPath);
        return result;
      }
      ec.clear();

      if (entry->IsDir()) {
        std::filesystem::create_directories(destinationPath, ec);
        if (ec) {
          AddDiagnostic(result, ArchiveDiagnosticCode::FilesystemError,
                        "Could not create GDTF extraction subdirectory.",
                        entryPath);
        }
        continue;
      }

      std::filesystem::create_directories(destinationPath.parent_path(), ec);
      if (ec) {
        AddDiagnostic(result, ArchiveDiagnosticCode::FilesystemError,
                      "Could not create GDTF extraction parent directory.",
                      entryPath);
        continue;
      }

      const std::filesystem::path normalizedDestination =
          std::filesystem::weakly_canonical(destinationPath.parent_path(), ec);
      if (ec || normalizedDestination.native().rfind(normalizedRoot.native(),
                                                     0) != 0) {
        AddDiagnostic(
            result, ArchiveDiagnosticCode::UnsafeEntryPath,
            "The GDTF archive entry would extract outside the destination.",
            entryPath);
        continue;
      }
      ec.clear();

      if (!WriteCurrentEntry(zipInput, destinationPath)) {
        AddDiagnostic(result, ArchiveDiagnosticCode::EntryReadFailed,
                      "Could not extract a GDTF archive entry.", entryPath);
      }
    }

    if (result.utf8FlagMissingEntryCount > 0) {
      AddDiagnostic(result, ArchiveDiagnosticCode::Utf8FallbackUsed,
                    "GDTF archive uses valid UTF-8 filenames without the ZIP "
                    "UTF-8 flag; compatibility fallback applied to " +
                        std::to_string(result.utf8FlagMissingEntryCount) +
                        " entries.");
    }
    if (result.entries.empty()) {
      AddDiagnostic(result, ArchiveDiagnosticCode::NoReadableEntries,
                    "The GDTF archive does not contain readable entries.");
    }
  } catch (const std::filesystem::filesystem_error &error) {
    AddDiagnostic(
        result, ArchiveDiagnosticCode::FilesystemError,
        std::string("Filesystem error while extracting GDTF archive: ") +
            error.what());
  } catch (const std::system_error &error) {
    AddDiagnostic(result, ArchiveDiagnosticCode::FilesystemError,
                  std::string("System error while extracting GDTF archive: ") +
                      error.what());
  } catch (const std::exception &error) {
    AddDiagnostic(
        result, ArchiveDiagnosticCode::UnexpectedException,
        std::string("Unexpected error while extracting GDTF archive: ") +
            error.what());
  } catch (...) {
    AddDiagnostic(result, ArchiveDiagnosticCode::UnexpectedException,
                  "Unknown error while extracting GDTF archive.");
  }
  return result;
}

} // namespace gdtf
