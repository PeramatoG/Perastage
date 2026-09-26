#include "inspection/gdtf_byte_source.h"

#include <fstream>

namespace perastage::inspection::internal {

// Publishes bounded owned bytes to a deterministic workspace-local filename.
GdtfByteSource::GdtfByteSource(std::span<const std::uint8_t> bytes)
    : workspace_("gdtf-inspection") {
  if (!workspace_.IsValid())
    return;
  path_ = workspace_.Path() / "inspection.gdtf";
  std::ofstream output(path_, std::ios::binary);
  if (!output)
    return;
  if (!bytes.empty())
    output.write(reinterpret_cast<const char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  output.close();
  valid_ = output.good();
}

// Reports whether all bytes were closed successfully before inspection.
bool GdtfByteSource::Valid() const { return valid_; }

// Returns the internal path used only by established GDTF readers.
const std::filesystem::path &GdtfByteSource::Path() const { return path_; }

} // namespace perastage::inspection::internal
