#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace symbol_cache {

inline constexpr int kCurrentPerastageSymbolFormatVersion = 1;

struct GdtfFileRevision {
  std::string physicalPathIdentity;
  std::uintmax_t fileSize = 0;
  std::int64_t modificationTime = 0;
  bool metadataAvailable = false;

  bool operator==(const GdtfFileRevision &) const = default;
  std::string Key() const;
};

struct SemanticFingerprintCacheStats {
  int hits = 0;
  int misses = 0;
};

struct GdtfSemanticFingerprintEntry {
  std::string normalizedPath;
  std::vector<std::uint8_t> bytes;
};

GdtfFileRevision ReadGdtfFileRevision(const std::string &path);
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

} // namespace symbol_cache
