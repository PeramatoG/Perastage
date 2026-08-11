#include "fixture_symbol_resource_revision.h"

#include "filesystem_path_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace symbol_cache {
namespace {

// Converts a UTF-8 filesystem path into a wxString using the native platform
// representation.
wxString WxStringFromUtf8Path(const std::string &path) {
  const std::filesystem::path nativePath = PathUtils::PathFromUtf8(path);
#ifdef _WIN32
  return wxString(nativePath.wstring());
#else
  return wxString::FromUTF8(nativePath.string());
#endif
}

// Normalizes a ZIP entry path for deterministic GDTF semantic fingerprinting.

std::string NormalizeGdtfEntryPath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (!path.empty() && path.front() == '/')
    path.erase(path.begin());
  std::string normalized;
  bool previousSlash = false;
  for (char ch : path) {
    if (ch == '/') {
      if (!previousSlash)
        normalized.push_back('/');
      previousSlash = true;
    } else {
      normalized.push_back(
          static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      previousSlash = false;
    }
  }
  return normalized;
}

// Reports whether a normalized GDTF entry affects generated Perastage symbols.
bool IsSymbolRelevantGdtfEntry(const std::string &path) {
  if (path == "description.xml")
    return true;
  constexpr std::array<const char *, 10> prefixes = {
      "models/svg/",      "models/svg_front/", "models/svg_side/",
      "models/gltf_low/", "models/gltf/",      "models/gltf_high/",
      "models/glb/",      "models/3ds_low/",   "models/3ds/",
      "models/3ds_high/"};
  for (const char *prefix : prefixes) {
    if (path.rfind(prefix, 0) == 0)
      return true;
  }
  return false;
}

// Updates an FNV-1a hash with raw bytes.
static void UpdateFnv1a(std::uint64_t &hash, const void *data, size_t size) {
  const auto *bytes = static_cast<const unsigned char *>(data);
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
}

struct FingerprintCacheEntry {
  std::uintmax_t fileSize = 0;
  std::filesystem::file_time_type lastWriteTime{};
  std::string fingerprint;
};

std::mutex g_fingerprintCacheMutex;
std::unordered_map<std::string, FingerprintCacheEntry> g_fingerprintCache;
SemanticFingerprintCacheStats g_fingerprintCacheStats;

// Returns the canonical cache path used for in-process semantic fingerprint
// memoization.
std::string NormalizeFingerprintCachePath(const std::string &path) {
  std::error_code ec;
  const std::filesystem::path fsPath =
      std::filesystem::weakly_canonical(path, ec);
  return (ec ? std::filesystem::absolute(path, ec) : fsPath).generic_string();
}

// Reads file metadata that must match before a cached semantic fingerprint is
// reused.
bool ReadFingerprintFileMetadata(
    const std::string &path, std::uintmax_t &fileSize,
    std::filesystem::file_time_type &lastWriteTime) {
  std::error_code ec;
  fileSize = std::filesystem::file_size(path, ec);
  if (ec)
    return false;
  lastWriteTime = std::filesystem::last_write_time(path, ec);
  return !ec;
}

} // namespace

// Builds the cheap, bounded revision used by hot symbol lookup paths.
GdtfFileRevision ReadGdtfFileRevision(const std::string &path) {
  GdtfFileRevision revision;
  revision.physicalPathIdentity = PathUtils::BuildFilesystemIdentityKey(path);
  std::error_code sizeError;
  revision.fileSize = std::filesystem::file_size(path, sizeError);
  std::error_code timeError;
  const auto writeTime = std::filesystem::last_write_time(path, timeError);
  if (!timeError) {
    revision.modificationTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            writeTime.time_since_epoch())
            .count();
  }
  revision.metadataAvailable = !sizeError && !timeError;
  return revision;
}

// Serializes the bounded file revision for use in an in-process cache key.
std::string GdtfFileRevision::Key() const {
  return physicalPathIdentity + "\n" + std::to_string(fileSize) + "\n" +
         std::to_string(modificationTime) + "\n" +
         (metadataAvailable ? "1" : "0");
}

// Computes a versioned semantic fingerprint over normalized symbol-relevant
// entries.
std::string ComputeGdtfSemanticFingerprintFromEntries(
    const std::vector<GdtfSemanticFingerprintEntry> &inputEntries,
    std::string &errorMessage) {
  std::map<std::string, std::vector<std::uint8_t>> entries;
  for (const auto &entry : inputEntries) {
    const std::string normalized = NormalizeGdtfEntryPath(entry.normalizedPath);
    if (IsSymbolRelevantGdtfEntry(normalized))
      entries[normalized] = entry.bytes;
  }

  if (entries.find("description.xml") == entries.end()) {
    errorMessage = "GDTF semantic fingerprint requires description.xml.";
    return {};
  }

  std::uint64_t hash = 1469598103934665603ull;
  std::uint64_t payloadSize = 0;
  constexpr char version[] = "gdtf-symbol-fnv1a64-v1\n";
  UpdateFnv1a(hash, version, sizeof(version) - 1);
  for (const auto &[name, bytes] : entries) {
    UpdateFnv1a(hash, name.data(), name.size());
    const char separator = '\0';
    UpdateFnv1a(hash, &separator, 1);
    if (!bytes.empty())
      UpdateFnv1a(hash, bytes.data(), bytes.size());
    UpdateFnv1a(hash, &separator, 1);
    payloadSize += static_cast<std::uint64_t>(name.size() + bytes.size());
  }

  std::ostringstream out;
  out << "gdtfsymfnv1a64v1:" << std::hex << std::setw(16) << std::setfill('0')
      << hash << ":" << std::dec << entries.size() << ":" << payloadSize;
  errorMessage.clear();
  return out.str();
}

// Computes a versioned semantic fingerprint over symbol-relevant uncompressed
// GDTF entries.
static std::string
ComputeGdtfSemanticFingerprintUncached(const std::string &path,
                                       std::string &errorMessage) {
  wxFileInputStream input(WxStringFromUtf8Path(path));
  if (!input.IsOk()) {
    errorMessage = "Could not open GDTF archive for semantic fingerprinting.";
    return {};
  }

  wxZipInputStream zip(input);
  if (!zip.IsOk()) {
    errorMessage = "Could not read GDTF archive for semantic fingerprinting.";
    return {};
  }

  std::vector<GdtfSemanticFingerprintEntry> entries;
  while (true) {
    std::unique_ptr<wxZipEntry> entry(zip.GetNextEntry());
    if (!entry)
      break;
    if (entry->IsDir())
      continue;
    const std::string normalized =
        NormalizeGdtfEntryPath(entry->GetName().ToStdString());
    if (!IsSymbolRelevantGdtfEntry(normalized))
      continue;

    GdtfSemanticFingerprintEntry fingerprintEntry;
    fingerprintEntry.normalizedPath = normalized;
    std::array<char, 8192> buffer{};
    while (true) {
      zip.Read(buffer.data(), buffer.size());
      const size_t count = zip.LastRead();
      if (count == 0)
        break;
      const auto *begin = reinterpret_cast<const std::uint8_t *>(buffer.data());
      fingerprintEntry.bytes.insert(fingerprintEntry.bytes.end(), begin,
                                    begin + count);
    }
    entries.push_back(std::move(fingerprintEntry));
  }

  return ComputeGdtfSemanticFingerprintFromEntries(entries, errorMessage);
}

// Computes a memoized semantic GDTF content fingerprint for unchanged files.
std::string ComputeGdtfSemanticFingerprint(const std::string &path,
                                           std::string &errorMessage) {
  const std::string cachePath = NormalizeFingerprintCachePath(path);
  std::uintmax_t fileSize = 0;
  std::filesystem::file_time_type lastWriteTime{};
  if (ReadFingerprintFileMetadata(path, fileSize, lastWriteTime)) {
    std::lock_guard<std::mutex> lock(g_fingerprintCacheMutex);
    const auto it = g_fingerprintCache.find(cachePath);
    if (it != g_fingerprintCache.end() && it->second.fileSize == fileSize &&
        it->second.lastWriteTime == lastWriteTime) {
      ++g_fingerprintCacheStats.hits;
      errorMessage.clear();
      return it->second.fingerprint;
    }
    ++g_fingerprintCacheStats.misses;
  }

  std::string fingerprint =
      ComputeGdtfSemanticFingerprintUncached(path, errorMessage);
  if (!fingerprint.empty() &&
      ReadFingerprintFileMetadata(path, fileSize, lastWriteTime)) {
    std::lock_guard<std::mutex> lock(g_fingerprintCacheMutex);
    g_fingerprintCache[cachePath] =
        FingerprintCacheEntry{fileSize, lastWriteTime, fingerprint};
  }
  return fingerprint;
}

// Publishes a known semantic fingerprint for the current on-disk file metadata.
void PublishGdtfSemanticFingerprintCache(const std::string &path,
                                         const std::string &fingerprint) {
  std::uintmax_t fileSize = 0;
  std::filesystem::file_time_type lastWriteTime{};
  if (fingerprint.empty() ||
      !ReadFingerprintFileMetadata(path, fileSize, lastWriteTime))
    return;
  std::lock_guard<std::mutex> lock(g_fingerprintCacheMutex);
  g_fingerprintCache[NormalizeFingerprintCachePath(path)] =
      FingerprintCacheEntry{fileSize, lastWriteTime, fingerprint};
}

// Invalidates the cached semantic fingerprint for one GDTF path.
void InvalidateGdtfSemanticFingerprintCache(const std::string &path) {
  std::lock_guard<std::mutex> lock(g_fingerprintCacheMutex);
  g_fingerprintCache.erase(NormalizeFingerprintCachePath(path));
}

// Clears all cached semantic fingerprints and cache counters.
void ClearGdtfSemanticFingerprintCache() {
  std::lock_guard<std::mutex> lock(g_fingerprintCacheMutex);
  g_fingerprintCache.clear();
  g_fingerprintCacheStats = {};
}

// Returns semantic fingerprint cache hit/miss counters.
SemanticFingerprintCacheStats GetGdtfSemanticFingerprintCacheStats() {
  std::lock_guard<std::mutex> lock(g_fingerprintCacheMutex);
  return g_fingerprintCacheStats;
}

} // namespace symbol_cache
