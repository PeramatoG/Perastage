#include "gdtf_source_fingerprint.h"

#include "filesystem_path_utils.h"

#include <chrono>
#include <cstdint>
#include <sstream>

namespace gui {

// Builds a stable source fingerprint for cached GDTF preview resources.
std::string BuildGdtfSourceFingerprint(const std::filesystem::path &path) {
  if (path.empty())
    return {};
  std::error_code canonicalError;
  const auto canonical = std::filesystem::weakly_canonical(path, canonicalError);
  std::error_code sizeError;
  const auto size = std::filesystem::file_size(path, sizeError);
  const std::uintmax_t normalizedSize = sizeError ? std::uintmax_t{0} : size;
  std::error_code timeError;
  const auto writeTime = std::filesystem::last_write_time(path, timeError);
  std::int64_t timestampNs = 0;
  if (!timeError) {
    timestampNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      writeTime.time_since_epoch())
                      .count();
  }
  std::ostringstream out;
  out << PathUtils::PathToUtf8(canonicalError ? path : canonical) << "|"
      << normalizedSize << "|" << timestampNs;
  return out.str();
}

} // namespace gui
