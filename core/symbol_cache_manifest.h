#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace symbol_cache {

inline constexpr int kCurrentManifestFormatVersion = 1;
inline constexpr int kCurrentPerastageSymbolFormatVersion = 1;
inline constexpr const char *kProjectArchiveEntryName =
    "perastage_symbol_cache_manifest.json";

struct ManifestArchiveResource {
  std::string entryName;
  std::vector<std::uint8_t> bytes;
};

struct FixtureSymbolCacheEntry {
  std::string fixtureKey;
  std::string fixtureTypeName;
  std::string gdtfSpec;
  std::string gdtfContentHash;
  bool hasPerastageSymbols = false;
  std::set<std::string> availableViews;
  std::string lastGenerationTimestampUtc;
};

enum class ValidationStatus {
  Valid,
  Bypassed,
  MissingManifest,
  UnknownManifestVersion,
  MissingEntry,
  OutdatedSymbolFormat,
  GdtfSpecChanged,
  GdtfHashChanged,
  MissingRequiredViews,
  SymbolsNotMarkedAvailable
};

struct ValidationRequest {
  std::string fixtureKey;
  std::string fixtureTypeName;
  std::string gdtfSpec;
  std::string gdtfContentHash;
  std::set<std::string> requiredViews;
  bool bypassManifest = false;
};

struct ValidationResult {
  ValidationStatus status = ValidationStatus::MissingManifest;
  bool valid = false;
  std::string message;
};

class SymbolCacheManifest {
public:
  void Clear();
  bool HasLoadedManifest() const;
  bool IsManifestFormatKnown() const;

  ValidationResult ValidateFixture(const ValidationRequest &request) const;
  void MarkFixtureSymbolsValid(const ValidationRequest &request,
                               std::string timestampUtc = {});

  bool LoadFromJsonText(const std::string &jsonText, std::string &errorMessage);
  bool SaveToJsonText(std::string &jsonText, std::string &errorMessage) const;

  bool LoadFromProjectArchive(const std::string &projectPath,
                              std::string &errorMessage);
  std::optional<ManifestArchiveResource> ToArchiveResource(
      std::string &errorMessage) const;

  const std::vector<FixtureSymbolCacheEntry> &Entries() const;

private:
  int manifestFormatVersion = kCurrentManifestFormatVersion;
  int perastageSymbolFormatVersion = kCurrentPerastageSymbolFormatVersion;
  bool loadedManifest = false;
  std::vector<FixtureSymbolCacheEntry> entries;
};

struct SemanticFingerprintCacheStats {
  int hits = 0;
  int misses = 0;
};

struct GdtfSemanticFingerprintEntry {
  std::string normalizedPath;
  std::vector<std::uint8_t> bytes;
};

std::string ComputeGdtfSemanticFingerprintFromEntries(
    const std::vector<GdtfSemanticFingerprintEntry> &entries,
    std::string &errorMessage);
std::string ComputeGdtfSemanticFingerprint(const std::string &path,
                                           std::string &errorMessage);
void PublishGdtfSemanticFingerprintCache(const std::string &path,
                                         const std::string &fingerprint);
void InvalidateGdtfSemanticFingerprintCache(const std::string &path);
void ClearGdtfSemanticFingerprintCache();
SemanticFingerprintCacheStats GetGdtfSemanticFingerprintCacheStats();
std::string ComputeFileContentHash(const std::string &path,
                                   std::string &errorMessage);
std::set<std::string> RequiredPerastageSymbolViews();
std::string CurrentUtcTimestamp();
const char *ValidationStatusName(ValidationStatus status);

} // namespace symbol_cache
