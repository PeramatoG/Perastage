#include "inspection/package_inspection.h"

#include "archive_entry_path.h"
#include "archive_zip_directory.h"

#include <algorithm>
#include <cctype>
#include <exception>

namespace perastage::inspection {
namespace {

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
bool ReadInventory(PackageInspectionResult &result, PackageInventory &inventory,
                   const archive::zip::DirectoryReadResult &directory) {
  if (directory.status == archive::zip::DirectoryReadStatus::OpenFailed) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
                  package_diagnostic_codes::OpenFailed,
                  "The input package could not be opened for reading.");
    return false;
  }
  if (directory.status == archive::zip::DirectoryReadStatus::Malformed) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  package_diagnostic_codes::MalformedArchive,
                  "The ZIP package directory could not be read.");
    return false;
  }
  if (!directory.Success()) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  package_diagnostic_codes::UnsupportedZipStructure,
                  "The ZIP package uses a structure that package inspection "
                  "does not currently support.");
    return false;
  }

  for (const archive::zip::DirectoryEntry &entry : directory.entries) {
    const std::string &rawName = entry.bytes;
    if (!archive::zip::IsValidUtf8(rawName)) {
      AddDiagnostic(result, DiagnosticSeverity::Error,
                    DiagnosticDomain::Package,
                    package_diagnostic_codes::FilenameDecodeFailed,
                    "A package entry filename could not be decoded as UTF-8.");
      continue;
    }

    PackageEntry item;
    item.displayPath = rawName;

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
        entry.directory ? PackageEntryType::Directory : PackageEntryType::File;
    item.uncompressedSize = entry.uncompressedSize;
    item.sizeKnown = true;
    inventory.entries.push_back(std::move(item));
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
  try {
    PackageInventory inventory;
    inventory.kind = *kind;
    if (ReadInventory(
            result, inventory,
            archive::zip::ReadDirectory(result.inspection.request.sourcePath)))
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

// Inventories one owned package buffer without writing it to the filesystem.
PackageInspectionResult InspectPackage(const std::vector<std::uint8_t> &bytes,
                                       PackageKind kind,
                                       const Request &request) {
  PackageInspectionResult result;
  result.inspection.request = request;
  PackageInventory inventory;
  inventory.kind = kind;
  try {
    if (ReadInventory(result, inventory, archive::zip::ReadDirectory(bytes)))
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

} // namespace perastage::inspection
