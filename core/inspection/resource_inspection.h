#pragma once

#include "inspection/package_inspection.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace perastage::inspection {

enum class ResourceKind : std::uint8_t {
  XmlText,
  Text,
  Image,
  Model,
  NestedGdtf,
  Binary,
};

// Describes one package entry for presentation-neutral browsing.
struct ResourceDescriptor {
  std::string displayPath;
  std::optional<std::string> normalizedPath;
  PackageKind packageKind = PackageKind::Gdtf;
  PackageEntryType entryType = PackageEntryType::File;
  std::uint64_t size = 0;
  bool sizeKnown = false;
  bool pathSafe = false;
  ResourceKind kind = ResourceKind::Binary;
  bool rawReadSupported = false;
  bool textPreviewSupported = false;
};

// Owns a bounded resource payload and neutral operation diagnostics.
struct ResourceReadResult {
  Result inspection;
  std::string requestedPath;
  std::string resolvedPath;
  std::vector<std::uint8_t> bytes;
  ResourceKind kind = ResourceKind::Binary;
  bool completed = false;

  bool Success() const;
};

// Owns an unchanged UTF-8 preview and neutral operation diagnostics.
struct TextPreviewResult {
  Result inspection;
  std::string resolvedPath;
  std::string text;
  bool completed = false;

  bool Success() const;
};

namespace resource_diagnostic_codes {
inline constexpr char InvalidReadLimit[] = "resource.invalid_read_limit";
inline constexpr char NotFound[] = "resource.not_found";
inline constexpr char Ambiguous[] = "resource.ambiguous";
inline constexpr char UnsafePath[] = "resource.unsafe_path";
inline constexpr char TooLarge[] = "resource.too_large";
inline constexpr char ReadFailed[] = "resource.read_failed";
inline constexpr char UnsupportedTextPreview[] =
    "resource.unsupported_text_preview";
inline constexpr char InvalidTextEncoding[] = "resource.invalid_text_encoding";
inline constexpr char InvalidNestedGdtf[] = "resource.invalid_nested_gdtf";
inline constexpr char CompatibilityFallback[] =
    "resource.compatibility_fallback";
} // namespace resource_diagnostic_codes

std::vector<ResourceDescriptor>
DescribePackageResources(const PackageInventory &inventory);
std::vector<ResourceDescriptor>
DescribePackageResources(const std::filesystem::path &packagePath,
                         const PackageInventory &inventory,
                         std::uint64_t maxSniffBytes);
std::vector<ResourceDescriptor>
DescribePackageResources(std::span<const std::uint8_t> packageBytes,
                         const PackageInventory &inventory,
                         std::uint64_t maxSniffBytes,
                         const Request &request = {});

ResourceReadResult ReadPackageResource(const std::filesystem::path &packagePath,
                                       PackageKind packageKind,
                                       const std::string &resourcePath,
                                       std::uint64_t maxBytes);
ResourceReadResult
ReadPackageResource(std::span<const std::uint8_t> packageBytes,
                    PackageKind packageKind, const std::string &resourcePath,
                    std::uint64_t maxBytes, const Request &request = {});

TextPreviewResult PreviewPackageText(const std::filesystem::path &packagePath,
                                     PackageKind packageKind,
                                     const std::string &resourcePath,
                                     std::uint64_t maxBytes);
TextPreviewResult PreviewPackageText(std::span<const std::uint8_t> packageBytes,
                                     PackageKind packageKind,
                                     const std::string &resourcePath,
                                     std::uint64_t maxBytes,
                                     const Request &request = {});

} // namespace perastage::inspection
