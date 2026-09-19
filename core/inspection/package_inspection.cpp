#include "inspection/package_inspection.h"

#include "archive_entry_path.h"
#include "wx_path_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <fstream>
#include <memory>
#include <vector>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace perastage::inspection {
namespace {
constexpr std::size_t kMaximumInventoryEntries = 65535;
constexpr std::size_t kMaximumEntryNameBytes = 64 * 1024;

// Reads a little-endian 16-bit integer from ZIP metadata.
std::uint16_t ReadLe16(const unsigned char *bytes) {
  return static_cast<std::uint16_t>(bytes[0]) |
         (static_cast<std::uint16_t>(bytes[1]) << 8);
}

// Reads a little-endian 32-bit integer from ZIP metadata.
std::uint32_t ReadLe32(const unsigned char *bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8) |
         (static_cast<std::uint32_t>(bytes[2]) << 16) |
         (static_cast<std::uint32_t>(bytes[3]) << 24);
}

// Folds ASCII letters for stable extension classification.
std::string LowerAscii(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

// Converts a filesystem extension to portable UTF-8 text.
std::string ExtensionUtf8(const std::filesystem::path &path) {
  const std::u8string extension = path.extension().u8string();
  return LowerAscii(std::string(extension.begin(), extension.end()));
}

// Appends one neutral diagnostic at the source package location.
void AddDiagnostic(PackageInspectionResult &result, DiagnosticSeverity severity,
                   DiagnosticDomain domain, const char *code,
                   std::string message,
                   std::optional<std::string> packageEntry = std::nullopt) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.domain = domain;
  diagnostic.code = code;
  diagnostic.message = std::move(message);
  DiagnosticLocation location;
  location.sourcePath = result.inspection.request.sourcePath;
  location.packageEntry = std::move(packageEntry);
  diagnostic.location = std::move(location);
  result.inspection.diagnostics.push_back(std::move(diagnostic));
}

// Classifies only the two package extensions explicitly supported by INS-100.
std::optional<PackageKind> ClassifyPackage(const std::filesystem::path &path) {
  const std::string extension = ExtensionUtf8(path);
  if (extension == ".gdtf")
    return PackageKind::Gdtf;
  if (extension == ".mvr")
    return PackageKind::Mvr;
  return std::nullopt;
}

// Checks for a ZIP record signature without reading or decompressing an entry.
bool HasZipRecordSignature(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::array<unsigned char, 4> signature{};
  input.read(reinterpret_cast<char *>(signature.data()), signature.size());
  if (input.gcount() != static_cast<std::streamsize>(signature.size()))
    return false;
  return signature[0] == 'P' && signature[1] == 'K' &&
         ((signature[2] == 3 && signature[3] == 4) ||
          (signature[2] == 5 && signature[3] == 6) ||
          (signature[2] == 7 && signature[3] == 8));
}

// Reads bounded raw central-directory names so wx path cleanup cannot hide
// unsafe roots.
std::optional<std::vector<std::string>>
ReadCentralDirectoryNames(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  input.seekg(0, std::ios::end);
  const std::streamoff fileSize = input.tellg();
  constexpr std::streamoff kMaximumTrailer = 22 + 0xffff;
  const std::streamoff trailerStart = std::max<std::streamoff>(
      0, fileSize > kMaximumTrailer ? fileSize - kMaximumTrailer : 0);
  const std::size_t trailerSize =
      static_cast<std::size_t>(fileSize - trailerStart);
  std::vector<unsigned char> trailer(trailerSize);
  input.seekg(trailerStart);
  input.read(reinterpret_cast<char *>(trailer.data()), trailer.size());
  if (!input || trailer.size() < 22)
    return std::nullopt;

  std::optional<std::size_t> endRecord;
  for (std::size_t offset = trailer.size() - 22;; --offset) {
    if (ReadLe32(trailer.data() + offset) == 0x06054b50) {
      endRecord = offset;
      break;
    }
    if (offset == 0)
      break;
  }
  if (!endRecord)
    return std::nullopt;

  const std::uint16_t entryCount = ReadLe16(trailer.data() + *endRecord + 10);
  const std::uint32_t directoryOffset =
      ReadLe32(trailer.data() + *endRecord + 16);
  std::vector<std::string> names;
  names.reserve(entryCount);
  input.clear();
  input.seekg(directoryOffset);
  for (std::uint16_t index = 0; index < entryCount; ++index) {
    std::array<unsigned char, 46> header{};
    input.read(reinterpret_cast<char *>(header.data()), header.size());
    if (!input || ReadLe32(header.data()) != 0x02014b50)
      return std::nullopt;
    const std::uint16_t nameSize = ReadLe16(header.data() + 28);
    const std::uint16_t extraSize = ReadLe16(header.data() + 30);
    const std::uint16_t commentSize = ReadLe16(header.data() + 32);
    std::string name(nameSize, '\0');
    input.read(name.data(), name.size());
    if (!input)
      return std::nullopt;
    names.push_back(std::move(name));
    input.seekg(static_cast<std::streamoff>(extraSize) + commentSize,
                std::ios::cur);
  }
  return names;
}

// Returns the lower-case extension of an archive entry's final component.
std::string EntryExtension(const std::string &normalizedPath) {
  const size_t slash = normalizedPath.find_last_of('/');
  const std::string name = slash == std::string::npos
                               ? normalizedPath
                               : normalizedPath.substr(slash + 1);
  const size_t dot = name.find_last_of('.');
  return dot == std::string::npos ? std::string()
                                  : LowerAscii(name.substr(dot));
}

// Reports the exact canonical root document name for a classified package.
const char *CanonicalRootDocument(PackageKind kind) {
  return kind == PackageKind::Gdtf ? "description.xml"
                                   : "GeneralSceneDescription.xml";
}

// Inventories ZIP metadata in archive order without reading entry payloads.
bool ReadInventory(PackageInspectionResult &result,
                   PackageInventory &inventory) {
  const std::optional<std::vector<std::string>> rawNames =
      ReadCentralDirectoryNames(result.inspection.request.sourcePath);
  if (!rawNames) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  package_diagnostic_codes::MalformedArchive,
                  "The ZIP package directory could not be read.");
    return false;
  }
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(
      result.inspection.request.sourcePath));
  if (!input.IsOk()) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
                  package_diagnostic_codes::OpenFailed,
                  "The input package could not be opened for reading.");
    return false;
  }

  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  std::size_t entryIndex = 0;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (inventory.entries.size() == kMaximumInventoryEntries) {
      AddDiagnostic(result, DiagnosticSeverity::Fatal,
                    DiagnosticDomain::Package,
                    package_diagnostic_codes::InventoryLimitExceeded,
                    "The package exceeds the Perastage inventory entry limit.");
      return false;
    }

    if (entryIndex >= rawNames->size()) {
      AddDiagnostic(result, DiagnosticSeverity::Fatal,
                    DiagnosticDomain::Package,
                    package_diagnostic_codes::MalformedArchive,
                    "The ZIP entry inventory is inconsistent.");
      return false;
    }
    const std::string &rawName = (*rawNames)[entryIndex++];
    const wxString decodedName =
        wxString::FromUTF8(rawName.data(), rawName.size());
    const wxScopedCharBuffer roundTrip = decodedName.utf8_str();
    if ((!rawName.empty() && decodedName.empty()) || !roundTrip.data() ||
        std::string(roundTrip.data(), roundTrip.length()) != rawName) {
      AddDiagnostic(result, DiagnosticSeverity::Error,
                    DiagnosticDomain::Package,
                    package_diagnostic_codes::FilenameDecodeFailed,
                    "A package entry filename could not be decoded as UTF-8.");
      continue;
    }

    PackageEntry item;
    item.displayPath = rawName;
    if (item.displayPath.size() > kMaximumEntryNameBytes) {
      AddDiagnostic(
          result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
          package_diagnostic_codes::InventoryLimitExceeded,
          "A package entry name exceeds the Perastage inventory limit.");
      return false;
    }

    const std::string normalized =
        archive::NormalizeEntrySeparators(item.displayPath);
    item.pathSafe = !archive::IsUnsafeNormalizedEntryPath(normalized);
    if (item.pathSafe) {
      item.normalizedPath = normalized;
      item.extension = EntryExtension(normalized);
      inventory.canonicalRootDocumentPresent |=
          normalized == CanonicalRootDocument(inventory.kind);
    } else {
      AddDiagnostic(result, DiagnosticSeverity::Error,
                    DiagnosticDomain::Package,
                    package_diagnostic_codes::UnsafeEntryPath,
                    "The package contains an unsafe archive entry path.",
                    item.displayPath);
    }

    item.type =
        entry->IsDir() ? PackageEntryType::Directory : PackageEntryType::File;
    const wxFileOffset size = entry->GetSize();
    if (size != wxInvalidOffset && size >= 0) {
      item.uncompressedSize = static_cast<std::uint64_t>(size);
      item.sizeKnown = true;
    }
    inventory.entries.push_back(std::move(item));
  }

  if (entryIndex != rawNames->size()) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  package_diagnostic_codes::MalformedArchive,
                  "The ZIP entry inventory is incomplete.");
    return false;
  }

  if (zip.GetLastError() != wxSTREAM_EOF &&
      zip.GetLastError() != wxSTREAM_NO_ERROR) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  package_diagnostic_codes::MalformedArchive,
                  "The ZIP package directory could not be read completely.");
    return false;
  }
  return true;
}
} // namespace

// Classifies and inventories one supported filesystem package without
// extraction.
PackageInspectionResult InspectPackage(const Request &request) {
  PackageInspectionResult result;
  result.inspection.request = request;
  if (request.sourcePath.empty()) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
                  package_diagnostic_codes::EmptyInputPath,
                  "The inspection input path is empty.");
    return result;
  }

  const std::optional<PackageKind> kind = ClassifyPackage(request.sourcePath);
  if (!kind) {
    AddDiagnostic(
        result, DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
        package_diagnostic_codes::UnsupportedFileType,
        "The input file type is not supported for package inspection.");
    return result;
  }

  std::error_code error;
  if (!std::filesystem::is_regular_file(request.sourcePath, error) || error) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
                  package_diagnostic_codes::OpenFailed,
                  "The input package is not a readable regular file.");
    return result;
  }
  if (!HasZipRecordSignature(request.sourcePath)) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  package_diagnostic_codes::MalformedArchive,
                  "The supported input is not a readable ZIP package.");
    return result;
  }

  try {
    PackageInventory inventory;
    inventory.kind = *kind;
    if (ReadInventory(result, inventory))
      result.inventory = std::move(inventory);
  } catch (const std::exception &) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  package_diagnostic_codes::UnexpectedReadFailure,
                  "An unexpected package read failure occurred.");
  } catch (...) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  package_diagnostic_codes::UnexpectedReadFailure,
                  "An unexpected package read failure occurred.");
  }
  return result;
}

// Wraps a filesystem path in the neutral inspection request contract.
PackageInspectionResult
InspectPackage(const std::filesystem::path &sourcePath) {
  return InspectPackage(Request{sourcePath});
}

} // namespace perastage::inspection
