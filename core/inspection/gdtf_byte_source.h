#pragma once

#include "runtime_storage.h"

#include <cstdint>
#include <filesystem>
#include <span>

namespace perastage::inspection::internal {

// Owns a GDTF byte adapter inside Perastage's audited runtime workspace.
class GdtfByteSource {
public:
  explicit GdtfByteSource(std::span<const std::uint8_t> bytes);

  bool Valid() const;
  const std::filesystem::path &Path() const;

private:
  runtime_storage::TemporaryWorkspace workspace_;
  std::filesystem::path path_;
  bool valid_ = false;
};

} // namespace perastage::inspection::internal
