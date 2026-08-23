#pragma once

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace project_cache {

inline constexpr const char *kFingerprintAlgorithm = "fnv1a64-v1";

class FingerprintAccumulator {
public:
  // Adds exact payload bytes to the versioned fingerprint.
  void Update(const void *data, size_t size) {
    const auto *bytes = static_cast<const std::uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
      hash_ ^= bytes[i];
      hash_ *= 1099511628211ull;
    }
  }

  // Returns the stable algorithm-qualified fingerprint text.
  std::string Finish() const {
    std::ostringstream out;
    out << kFingerprintAlgorithm << ':' << std::hex << std::setfill('0')
        << std::setw(16) << hash_;
    return out.str();
  }

private:
  std::uint64_t hash_ = 14695981039346656037ull;
};

struct NamedPayloadFingerprint {
  std::string normalizedEntryName;
  std::string contentFingerprint;
};

// Aggregates named resource fingerprints independently of archive order.
inline std::string AggregateNamedPayloadFingerprints(
    std::vector<NamedPayloadFingerprint> entries) {
  std::sort(entries.begin(), entries.end(),
            [](const auto &left, const auto &right) {
              return left.normalizedEntryName < right.normalizedEntryName;
            });
  FingerprintAccumulator accumulator;
  for (const auto &entry : entries) {
    accumulator.Update(entry.normalizedEntryName.data(),
                       entry.normalizedEntryName.size());
    const char separator = '\0';
    accumulator.Update(&separator, 1);
    accumulator.Update(entry.contentFingerprint.data(),
                       entry.contentFingerprint.size());
    accumulator.Update(&separator, 1);
  }
  return accumulator.Finish();
}

struct ValidationContext {
  std::string scenePackageFingerprint;
  std::string packagedLayoutResourceFingerprint;
  bool scenePackageCovered = false;
  bool packagedLayoutResourcesCovered = false;

  // Reports whether packaged rendering dependencies have authoritative proof.
  bool HasCompletePackageCoverage() const {
    return scenePackageCovered && packagedLayoutResourcesCovered &&
           !scenePackageFingerprint.empty() &&
           !packagedLayoutResourceFingerprint.empty();
  }
};

// Computes a fingerprint directly from an already-owned payload.
inline std::string FingerprintBytes(const void *data, size_t size) {
  FingerprintAccumulator accumulator;
  accumulator.Update(data, size);
  return accumulator.Finish();
}

} // namespace project_cache
