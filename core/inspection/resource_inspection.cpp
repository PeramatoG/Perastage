#include "inspection/resource_inspection.h"

#include "archive_entry_path.h"
#include "archive_zip_directory.h"
#include "archive_zip_entry_reader.h"
#include "gdtf_archive_reader.h"
#include "inspection/gdtf_byte_source.h"

#include <algorithm>
#include <cctype>
#include <exception>

namespace perastage::inspection {
namespace {
constexpr std::uint64_t kMaximumResourceReadBytes = 256ull * 1024ull * 1024ull;
// Sixty-four bytes cover supported signatures and bound classic-ZIP sniff
// storage.
constexpr std::uint64_t kMaximumResourceSniffBytes = 64;

// Folds ASCII letters for deterministic resource comparisons.
std::string LowerAscii(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

// Returns the lower-case extension from one normalized archive path.
std::string Extension(const std::string &path) {
  const std::size_t slash = path.find_last_of('/');
  const std::size_t dot = path.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
    return {};
  return LowerAscii(path.substr(dot));
}

// Appends one resource-operation diagnostic with package-entry context.
void AddDiagnostic(Result &result, DiagnosticSeverity severity,
                   const char *code, std::string message,
                   const std::string &entry,
                   DiagnosticClassification classification =
                       DiagnosticClassification::General) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.domain = DiagnosticDomain::Content;
  diagnostic.classification = classification;
  diagnostic.code = code;
  diagnostic.message = std::move(message);
  diagnostic.location = DiagnosticLocation{result.request.sourcePath, entry};
  result.diagnostics.push_back(std::move(diagnostic));
}

// Identifies supported content conservatively from bytes and extension hints.
ResourceKind IdentifyKind(const std::string &path,
                          std::span<const std::uint8_t> bytes) {
  const std::string extension = Extension(path);
  const bool zip = bytes.size() >= 4 && bytes[0] == 'P' && bytes[1] == 'K' &&
                   bytes[2] == 3 && bytes[3] == 4;
  if (zip && extension == ".gdtf")
    return ResourceKind::NestedGdtf;
  const bool png = bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 'P' &&
                   bytes[2] == 'N' && bytes[3] == 'G' && bytes[4] == '\r' &&
                   bytes[5] == '\n' && bytes[6] == 0x1a && bytes[7] == '\n';
  const bool jpeg = bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 &&
                    bytes[2] == 0xff;
  if (png || jpeg)
    return ResourceKind::Image;
  std::size_t textStart = 0;
  if (bytes.size() >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb &&
      bytes[2] == 0xbf)
    textStart = 3;
  while (textStart < bytes.size() &&
         std::isspace(static_cast<unsigned char>(bytes[textStart])))
    ++textStart;
  const bool xml =
      bytes.size() - textStart >= 2 && bytes[textStart] == '<' &&
      (bytes[textStart + 1] == '?' || bytes[textStart + 1] == '!' ||
       std::isalpha(static_cast<unsigned char>(bytes[textStart + 1])));
  if (xml && extension == ".xml")
    return ResourceKind::XmlText;
  if (bytes.size() >= 4 && bytes[0] == 'g' && bytes[1] == 'l' &&
      bytes[2] == 'T' && bytes[3] == 'F')
    return ResourceKind::Model;
  if (extension == ".3ds" && bytes.size() >= 2 && bytes[0] == 0x4d &&
      bytes[1] == 0x4d)
    return ResourceKind::Model;
  if ((extension == ".txt" || extension == ".svg") &&
      std::find(bytes.begin(), bytes.end(), 0) == bytes.end() &&
      archive::zip::IsValidUtf8(std::string(
          reinterpret_cast<const char *>(bytes.data()), bytes.size())))
    return extension == ".svg" && xml ? ResourceKind::XmlText
                                      : ResourceKind::Text;
  return ResourceKind::Binary;
}

// Resolves one exact safe entry through authoritative central-directory facts.
std::optional<std::size_t>
SelectGenericEntry(const archive::zip::DirectoryReadResult &directory,
                   const std::string &normalized, ResourceReadResult &result) {
  std::vector<std::size_t> exactMatches;
  std::vector<std::size_t> caseMatches;
  for (std::size_t index = 0; index < directory.entries.size(); ++index) {
    const archive::zip::DirectoryEntry &entry = directory.entries[index];
    if (!archive::zip::IsValidUtf8(entry.bytes) || entry.directory)
      continue;
    const std::string path = archive::NormalizeEntrySeparators(entry.bytes);
    if (!archive::IsUnsafeNormalizedEntryPath(path) &&
        LowerAscii(path) == LowerAscii(normalized)) {
      caseMatches.push_back(index);
      if (path == normalized)
        exactMatches.push_back(index);
    }
  }
  if (exactMatches.empty() && caseMatches.size() <= 1) {
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::NotFound,
                  "The requested package resource was not found.", normalized);
    return std::nullopt;
  }
  if (exactMatches.size() != 1 || caseMatches.size() != 1) {
    AddDiagnostic(
        result.inspection, DiagnosticSeverity::Error,
        resource_diagnostic_codes::Ambiguous,
        "The requested package resource is ambiguous or not an exact match.",
        normalized);
    return std::nullopt;
  }
  return exactMatches.front();
}

// Converts a bounded indexed-entry read into a neutral resource result.
void ApplyEntryRead(const archive::zip::EntryReadResult &read,
                    const std::string &normalized, ResourceReadResult &result) {
  if (read.status == archive::zip::EntryReadStatus::EntryTooLarge) {
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::TooLarge,
                  "The requested resource exceeds the safe read limit.",
                  normalized);
    return;
  }
  if (!read.Success()) {
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::ReadFailed,
                  "The requested package resource could not be read.",
                  normalized);
    return;
  }
  result.resolvedPath = normalized;
  result.bytes = read.bytes;
  result.kind = IdentifyKind(normalized, result.bytes);
  result.completed = true;
}

// Validates a generic resource request before authoritative entry selection.
bool PrepareGenericRead(const std::string &resourcePath, std::uint64_t maxBytes,
                        const Request &request, ResourceReadResult &result,
                        std::string &normalized) {
  result.inspection.request = request;
  result.requestedPath = resourcePath;
  normalized = archive::NormalizeEntrySeparators(resourcePath);
  if (maxBytes == 0 || maxBytes > kMaximumResourceReadBytes) {
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::InvalidReadLimit,
                  "The requested resource read limit is invalid.", normalized);
    return false;
  }
  if (normalized.empty() || archive::IsUnsafeNormalizedEntryPath(normalized)) {
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::UnsafePath,
                  "The requested resource path is unsafe.", normalized);
    return false;
  }
  return true;
}

// Converts an established GDTF read result without changing lookup policy.
ResourceReadResult ConvertGdtfRead(const gdtf::GdtfResourceReadResult &source,
                                   const Request &request) {
  ResourceReadResult result;
  result.inspection.request = request;
  result.requestedPath = source.requestedPath;
  result.resolvedPath = source.entryPath;
  result.bytes.assign(source.bytes.begin(), source.bytes.end());
  result.kind = IdentifyKind(source.entryPath, result.bytes);
  for (const auto &finding : source.diagnostics) {
    const char *code = resource_diagnostic_codes::ReadFailed;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    DiagnosticClassification classification = DiagnosticClassification::General;
    if (finding.code == gdtf::ArchiveDiagnosticCode::ResourceNotFound)
      code = resource_diagnostic_codes::NotFound;
    else if (finding.code == gdtf::ArchiveDiagnosticCode::ResourcePathAmbiguous)
      code = resource_diagnostic_codes::Ambiguous;
    else if (finding.code == gdtf::ArchiveDiagnosticCode::UnsafeResourcePath)
      code = resource_diagnostic_codes::UnsafePath;
    else if (finding.code == gdtf::ArchiveDiagnosticCode::ResourceEntryTooLarge)
      code = resource_diagnostic_codes::TooLarge;
    else if (finding.code == gdtf::ArchiveDiagnosticCode::Utf8FallbackUsed ||
             finding.code == gdtf::ArchiveDiagnosticCode::Utf8FlagMissing ||
             finding.code ==
                 gdtf::ArchiveDiagnosticCode::LegacyFilenameEncodingUsed) {
      code = resource_diagnostic_codes::CompatibilityFallback;
      severity = DiagnosticSeverity::Warning;
      classification = DiagnosticClassification::Compatibility;
    }
    AddDiagnostic(result.inspection, severity, code, finding.message,
                  finding.entryPath, classification);
  }
  result.completed = source.Success();
  return result;
}

// Converts a bounded resource read into an unchanged UTF-8 text preview.
TextPreviewResult MakeTextPreview(ResourceReadResult read) {
  TextPreviewResult result;
  const bool readSucceeded = read.Success();
  result.inspection = std::move(read.inspection);
  result.resolvedPath = std::move(read.resolvedPath);
  result.completed = readSucceeded;
  if (!readSucceeded)
    return result;
  if (read.kind != ResourceKind::XmlText && read.kind != ResourceKind::Text) {
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::UnsupportedTextPreview,
                  "The requested resource is not a supported text resource.",
                  result.resolvedPath);
    return result;
  }
  result.text.assign(reinterpret_cast<const char *>(read.bytes.data()),
                     read.bytes.size());
  if (!archive::zip::IsValidUtf8(result.text)) {
    result.completed = false;
    result.text.clear();
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::InvalidTextEncoding,
                  "The requested text resource is not valid UTF-8.",
                  result.resolvedPath);
  }
  return result;
}
} // namespace

// Reports whether a resource operation produced a selected payload.
bool ResourceReadResult::Success() const {
  return completed && !resolvedPath.empty() &&
         std::none_of(inspection.diagnostics.begin(),
                      inspection.diagnostics.end(), [](const Diagnostic &item) {
                        return item.severity >= DiagnosticSeverity::Error;
                      });
}

// Reports whether a resource operation produced supported text.
bool TextPreviewResult::Success() const {
  return completed && !resolvedPath.empty() &&
         std::none_of(inspection.diagnostics.begin(),
                      inspection.diagnostics.end(), [](const Diagnostic &item) {
                        return item.severity >= DiagnosticSeverity::Error;
                      });
}

// Adapts the authoritative package inventory into browseable descriptors.
std::vector<ResourceDescriptor>
DescribePackageResources(const PackageInventory &inventory) {
  std::vector<ResourceDescriptor> resources;
  resources.reserve(inventory.entries.size());
  for (const PackageEntry &entry : inventory.entries) {
    ResourceDescriptor resource;
    resource.displayPath = entry.displayPath;
    resource.normalizedPath = entry.normalizedPath;
    resource.packageKind = inventory.kind;
    resource.entryType = entry.type;
    resource.size = entry.uncompressedSize;
    resource.sizeKnown = entry.sizeKnown;
    resource.pathSafe = entry.pathSafe;
    resource.rawReadSupported = entry.pathSafe && entry.normalizedPath &&
                                entry.type == PackageEntryType::File;
    resources.push_back(std::move(resource));
  }
  return resources;
}

// Adds bounded filesystem signature facts to authoritative descriptors.
std::vector<ResourceDescriptor>
DescribePackageResources(const std::filesystem::path &packagePath,
                         const PackageInventory &inventory) {
  return DescribePackageResources(packagePath, inventory,
                                  kMaximumResourceSniffBytes);
}

// Adds explicitly bounded filesystem signature facts to descriptors.
std::vector<ResourceDescriptor>
DescribePackageResources(const std::filesystem::path &packagePath,
                         const PackageInventory &inventory,
                         std::uint64_t maxSniffBytes) {
  std::vector<ResourceDescriptor> resources =
      DescribePackageResources(inventory);
  const archive::zip::DirectoryReadResult directory =
      archive::zip::ReadDirectory(packagePath);
  if (!directory.Success() || maxSniffBytes == 0)
    return resources;
  const std::uint64_t sniffBytes =
      std::min(maxSniffBytes, kMaximumResourceSniffBytes);
  std::vector<std::optional<std::size_t>> prefixIndices(resources.size());
  std::vector<archive::zip::DirectoryEntry> selectedEntries;
  for (std::size_t resourceIndex = 0; resourceIndex < resources.size();
       ++resourceIndex) {
    ResourceDescriptor &resource = resources[resourceIndex];
    if (!resource.rawReadSupported || !resource.normalizedPath)
      continue;
    ResourceReadResult selection;
    const std::optional<std::size_t> index =
        SelectGenericEntry(directory, *resource.normalizedPath, selection);
    if (!index)
      continue;
    prefixIndices[resourceIndex] = selectedEntries.size();
    selectedEntries.push_back(directory.entries[*index]);
  }
  const std::vector<archive::zip::EntryReadResult> prefixes =
      archive::zip::ReadEntryPrefixes(packagePath, selectedEntries, sniffBytes);
  for (std::size_t resourceIndex = 0; resourceIndex < resources.size();
       ++resourceIndex) {
    ResourceDescriptor &resource = resources[resourceIndex];
    if (!prefixIndices[resourceIndex])
      continue;
    const archive::zip::EntryReadResult &prefix =
        prefixes[*prefixIndices[resourceIndex]];
    if (!prefix.Success())
      continue;
    resource.kind = IdentifyKind(*resource.normalizedPath, prefix.bytes);
    if (!prefix.complete && (resource.kind == ResourceKind::XmlText ||
                             resource.kind == ResourceKind::Text))
      resource.kind = ResourceKind::Binary;
    resource.textPreviewSupported = resource.kind == ResourceKind::XmlText ||
                                    resource.kind == ResourceKind::Text;
  }
  return resources;
}

// Adds bounded owned-buffer signature facts to authoritative descriptors.
std::vector<ResourceDescriptor>
DescribePackageResources(std::span<const std::uint8_t> packageBytes,
                         const PackageInventory &inventory,
                         std::uint64_t maxSniffBytes, const Request &request) {
  std::vector<ResourceDescriptor> resources =
      DescribePackageResources(inventory);
  const archive::zip::DirectoryReadResult directory =
      archive::zip::ReadDirectory(packageBytes);
  if (!directory.Success() || maxSniffBytes == 0)
    return resources;
  const std::uint64_t sniffBytes =
      std::min(maxSniffBytes, kMaximumResourceSniffBytes);
  std::vector<std::optional<std::size_t>> prefixIndices(resources.size());
  std::vector<archive::zip::DirectoryEntry> selectedEntries;
  for (std::size_t resourceIndex = 0; resourceIndex < resources.size();
       ++resourceIndex) {
    ResourceDescriptor &resource = resources[resourceIndex];
    if (!resource.rawReadSupported || !resource.normalizedPath)
      continue;
    ResourceReadResult selection;
    selection.inspection.request = request;
    const std::optional<std::size_t> index =
        SelectGenericEntry(directory, *resource.normalizedPath, selection);
    if (!index)
      continue;
    prefixIndices[resourceIndex] = selectedEntries.size();
    selectedEntries.push_back(directory.entries[*index]);
  }
  const std::vector<archive::zip::EntryReadResult> prefixes =
      archive::zip::ReadEntryPrefixes(packageBytes, selectedEntries,
                                      sniffBytes);
  for (std::size_t resourceIndex = 0; resourceIndex < resources.size();
       ++resourceIndex) {
    ResourceDescriptor &resource = resources[resourceIndex];
    if (!prefixIndices[resourceIndex])
      continue;
    const archive::zip::EntryReadResult &prefix =
        prefixes[*prefixIndices[resourceIndex]];
    if (!prefix.Success())
      continue;
    resource.kind = IdentifyKind(*resource.normalizedPath, prefix.bytes);
    if (!prefix.complete && (resource.kind == ResourceKind::XmlText ||
                             resource.kind == ResourceKind::Text))
      resource.kind = ResourceKind::Binary;
    resource.textPreviewSupported = resource.kind == ResourceKind::XmlText ||
                                    resource.kind == ResourceKind::Text;
  }
  return resources;
}

// Reads one filesystem package resource through the applicable lookup policy.
ResourceReadResult ReadPackageResource(const std::filesystem::path &packagePath,
                                       PackageKind packageKind,
                                       const std::string &resourcePath,
                                       std::uint64_t maxBytes) {
  const Request request{packagePath};
  ResourceReadResult result;
  std::string normalized;
  try {
    if (!PrepareGenericRead(resourcePath, maxBytes, request, result,
                            normalized))
      return result;
    if (packageKind == PackageKind::Gdtf)
      return ConvertGdtfRead(
          gdtf::ReadGdtfArchiveResource(packagePath, resourcePath, maxBytes),
          request);
    const archive::zip::DirectoryReadResult directory =
        archive::zip::ReadDirectory(packagePath);
    if (!directory.Success()) {
      AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                    resource_diagnostic_codes::ReadFailed,
                    "The package directory could not be read.", normalized);
      return result;
    }
    const std::optional<std::size_t> index =
        SelectGenericEntry(directory, normalized, result);
    if (!index)
      return result;
    if (directory.entries[*index].uncompressedSize > maxBytes) {
      AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                    resource_diagnostic_codes::TooLarge,
                    "The requested resource exceeds the safe read limit.",
                    normalized);
      return result;
    }
    ApplyEntryRead(archive::zip::ReadEntry(packagePath,
                                           directory.entries[*index], maxBytes),
                   normalized, result);
  } catch (const std::exception &) {
    ResourceReadResult result;
    result.inspection.request = request;
    result.requestedPath = resourcePath;
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::ReadFailed,
                  "The package resource could not be read.", resourcePath);
    return result;
  }
  return result;
}

// Reads one owned-buffer package resource without filesystem extraction.
ResourceReadResult
ReadPackageResource(std::span<const std::uint8_t> packageBytes,
                    PackageKind packageKind, const std::string &resourcePath,
                    std::uint64_t maxBytes, const Request &request) {
  ResourceReadResult result;
  std::string normalized;
  try {
    if (!PrepareGenericRead(resourcePath, maxBytes, request, result,
                            normalized))
      return result;
    if (packageKind == PackageKind::Gdtf) {
      internal::GdtfByteSource source(packageBytes);
      if (!source.Valid()) {
        AddDiagnostic(
            result.inspection, DiagnosticSeverity::Error,
            resource_diagnostic_codes::ReadFailed,
            "A workspace for GDTF resource reading could not be created.",
            normalized);
        return result;
      }
      return ConvertGdtfRead(
          gdtf::ReadGdtfArchiveResource(source.Path(), resourcePath, maxBytes),
          request);
    }
    const archive::zip::DirectoryReadResult directory =
        archive::zip::ReadDirectory(packageBytes);
    if (!directory.Success()) {
      AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                    resource_diagnostic_codes::ReadFailed,
                    "The package directory could not be read.", normalized);
      return result;
    }
    const std::optional<std::size_t> index =
        SelectGenericEntry(directory, normalized, result);
    if (!index)
      return result;
    if (directory.entries[*index].uncompressedSize > maxBytes) {
      AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                    resource_diagnostic_codes::TooLarge,
                    "The requested resource exceeds the safe read limit.",
                    normalized);
      return result;
    }
    ApplyEntryRead(archive::zip::ReadEntry(packageBytes,
                                           directory.entries[*index], maxBytes),
                   normalized, result);
  } catch (const std::exception &) {
    result.inspection.request = request;
    result.requestedPath = resourcePath;
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::ReadFailed,
                  "The package resource could not be read.", resourcePath);
  }
  return result;
}

// Returns an unchanged bounded UTF-8 preview from a filesystem package.
TextPreviewResult PreviewPackageText(const std::filesystem::path &packagePath,
                                     PackageKind packageKind,
                                     const std::string &resourcePath,
                                     std::uint64_t maxBytes) {
  return MakeTextPreview(
      ReadPackageResource(packagePath, packageKind, resourcePath, maxBytes));
}

// Returns an unchanged bounded UTF-8 preview from an owned package buffer.
TextPreviewResult PreviewPackageText(std::span<const std::uint8_t> packageBytes,
                                     PackageKind packageKind,
                                     const std::string &resourcePath,
                                     std::uint64_t maxBytes,
                                     const Request &request) {
  return MakeTextPreview(ReadPackageResource(packageBytes, packageKind,
                                             resourcePath, maxBytes, request));
}

} // namespace perastage::inspection
