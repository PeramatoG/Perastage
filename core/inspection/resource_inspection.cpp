#include "inspection/resource_inspection.h"

#include "archive_entry_path.h"
#include "archive_zip_directory.h"
#include "gdtf_archive_reader.h"
#include "wx_path_utils.h"

#include <algorithm>
#include <cctype>
#include <memory>

#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace perastage::inspection {
namespace {
constexpr std::uint64_t kMaximumResourceReadBytes = 256ull * 1024ull * 1024ull;

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
                   const std::string &entry) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.domain = DiagnosticDomain::Content;
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

// Reads one selected entry while enforcing the limit during decompression.
template <typename InputStream>
ResourceReadResult
ReadGenericResource(InputStream &input, const std::string &resourcePath,
                    std::uint64_t maxBytes, const Request &request) {
  ResourceReadResult result;
  result.inspection.request = request;
  result.requestedPath = resourcePath;
  const std::string normalized =
      archive::NormalizeEntrySeparators(resourcePath);
  if (maxBytes == 0 || maxBytes > kMaximumResourceReadBytes) {
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::InvalidReadLimit,
                  "The requested resource read limit is invalid.", normalized);
    return result;
  }
  if (normalized.empty() || archive::IsUnsafeNormalizedEntryPath(normalized)) {
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::UnsafePath,
                  "The requested resource path is unsafe.", normalized);
    return result;
  }

  std::vector<std::string> matchingPaths;
  {
    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> inventoryEntry;
    while ((inventoryEntry.reset(zip.GetNextEntry())), inventoryEntry) {
      const wxScopedCharBuffer utf8 = inventoryEntry->GetName().ToUTF8();
      if (!utf8 || inventoryEntry->IsDir())
        continue;
      const std::string path = archive::NormalizeEntrySeparators(utf8.data());
      if (!archive::IsUnsafeNormalizedEntryPath(path) &&
          LowerAscii(path) == LowerAscii(normalized))
        matchingPaths.push_back(path);
    }
  }
  if (matchingPaths.empty()) {
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::NotFound,
                  "The requested package resource was not found.", normalized);
    return result;
  }
  if (matchingPaths.size() != 1 || matchingPaths.front() != normalized) {
    AddDiagnostic(
        result.inspection, DiagnosticSeverity::Error,
        resource_diagnostic_codes::Ambiguous,
        "The requested package resource is ambiguous or not an exact match.",
        normalized);
    return result;
  }

  input.SeekI(0);
  wxZipInputStream dataZip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(dataZip.GetNextEntry())), entry) {
    const wxScopedCharBuffer utf8 = entry->GetName().ToUTF8();
    if (!utf8 || archive::NormalizeEntrySeparators(utf8.data()) != normalized)
      continue;
    const wxFileOffset claimedSize = entry->GetSize();
    if (claimedSize >= 0 &&
        static_cast<std::uint64_t>(claimedSize) > maxBytes) {
      AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                    resource_diagnostic_codes::TooLarge,
                    "The requested resource exceeds the safe read limit.",
                    normalized);
      return result;
    }
    std::uint8_t buffer[8192];
    while (true) {
      dataZip.Read(buffer, sizeof(buffer));
      const std::size_t count = dataZip.LastRead();
      if (count == 0)
        break;
      if (count > maxBytes || result.bytes.size() > maxBytes - count) {
        result.bytes.clear();
        AddDiagnostic(
            result.inspection, DiagnosticSeverity::Error,
            resource_diagnostic_codes::TooLarge,
            "The resource payload exceeded the safe read limit while reading.",
            normalized);
        return result;
      }
      result.bytes.insert(result.bytes.end(), buffer, buffer + count);
    }
    if (dataZip.GetLastError() != wxSTREAM_NO_ERROR &&
        dataZip.GetLastError() != wxSTREAM_EOF) {
      result.bytes.clear();
      AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                    resource_diagnostic_codes::ReadFailed,
                    "The requested package resource could not be read.",
                    normalized);
      return result;
    }
    result.resolvedPath = normalized;
    result.kind = IdentifyKind(normalized, result.bytes);
    return result;
  }
  AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                resource_diagnostic_codes::ReadFailed,
                "The selected package resource could not be reopened.",
                normalized);
  return result;
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
    if (finding.code == gdtf::ArchiveDiagnosticCode::ResourceNotFound)
      code = resource_diagnostic_codes::NotFound;
    else if (finding.code == gdtf::ArchiveDiagnosticCode::ResourcePathAmbiguous)
      code = resource_diagnostic_codes::Ambiguous;
    else if (finding.code == gdtf::ArchiveDiagnosticCode::UnsafeResourcePath)
      code = resource_diagnostic_codes::UnsafePath;
    else if (finding.code == gdtf::ArchiveDiagnosticCode::ResourceEntryTooLarge)
      code = resource_diagnostic_codes::TooLarge;
    AddDiagnostic(result.inspection,
                  finding.code == gdtf::ArchiveDiagnosticCode::Utf8FallbackUsed
                      ? DiagnosticSeverity::Warning
                      : DiagnosticSeverity::Error,
                  code, finding.message, finding.entryPath);
  }
  return result;
}

// Converts a bounded resource read into an unchanged UTF-8 text preview.
TextPreviewResult MakeTextPreview(ResourceReadResult read) {
  TextPreviewResult result;
  const bool readSucceeded = read.Success();
  result.inspection = std::move(read.inspection);
  result.resolvedPath = std::move(read.resolvedPath);
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
  return !resolvedPath.empty() && inspection.Success();
}

// Reports whether a resource operation produced supported text.
bool TextPreviewResult::Success() const {
  return !resolvedPath.empty() && !text.empty() && inspection.Success();
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
                         const PackageInventory &inventory,
                         std::uint64_t maxSniffBytes) {
  std::vector<ResourceDescriptor> resources =
      DescribePackageResources(inventory);
  for (ResourceDescriptor &resource : resources) {
    if (!resource.rawReadSupported || !resource.normalizedPath ||
        (resource.sizeKnown && resource.size > maxSniffBytes))
      continue;
    const ResourceReadResult read = ReadPackageResource(
        packagePath, inventory.kind, *resource.normalizedPath, maxSniffBytes);
    if (!read.Success())
      continue;
    resource.kind = read.kind;
    resource.textPreviewSupported =
        read.kind == ResourceKind::XmlText || read.kind == ResourceKind::Text;
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
  for (ResourceDescriptor &resource : resources) {
    if (!resource.rawReadSupported || !resource.normalizedPath ||
        (resource.sizeKnown && resource.size > maxSniffBytes))
      continue;
    const ResourceReadResult read =
        ReadPackageResource(packageBytes, inventory.kind,
                            *resource.normalizedPath, maxSniffBytes, request);
    if (!read.Success())
      continue;
    resource.kind = read.kind;
    resource.textPreviewSupported =
        read.kind == ResourceKind::XmlText || read.kind == ResourceKind::Text;
  }
  return resources;
}

// Reads one filesystem package resource through the applicable lookup policy.
ResourceReadResult ReadPackageResource(const std::filesystem::path &packagePath,
                                       PackageKind packageKind,
                                       const std::string &resourcePath,
                                       std::uint64_t maxBytes) {
  const Request request{packagePath};
  if (maxBytes == 0 || maxBytes > kMaximumResourceReadBytes) {
    ResourceReadResult result;
    result.inspection.request = request;
    result.requestedPath = resourcePath;
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::InvalidReadLimit,
                  "The requested resource read limit is invalid.",
                  resourcePath);
    return result;
  }
  if (packageKind == PackageKind::Gdtf)
    return ConvertGdtfRead(
        gdtf::ReadGdtfArchiveResource(packagePath, resourcePath, maxBytes),
        request);
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(packagePath));
  if (!input.IsOk()) {
    ResourceReadResult result;
    result.inspection.request = request;
    AddDiagnostic(result.inspection, DiagnosticSeverity::Error,
                  resource_diagnostic_codes::ReadFailed,
                  "The package could not be opened for resource reading.",
                  resourcePath);
    return result;
  }
  return ReadGenericResource(input, resourcePath, maxBytes, request);
}

// Reads one owned-buffer package resource without filesystem extraction.
ResourceReadResult
ReadPackageResource(std::span<const std::uint8_t> packageBytes, PackageKind,
                    const std::string &resourcePath, std::uint64_t maxBytes,
                    const Request &request) {
  wxMemoryInputStream input(packageBytes.data(), packageBytes.size());
  return ReadGenericResource(input, resourcePath, maxBytes, request);
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
