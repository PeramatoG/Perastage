#pragma once

#include "inspection/resource_inspection.h"

#include <cstdint>
#include <string>
#include <optional>

namespace gui::inspection {

inline constexpr std::uint64_t kInspectorImagePreviewBytes = 16U * 1024U * 1024U;
inline constexpr std::uint64_t kInspectorModelPreviewBytes = 64U * 1024U * 1024U;
inline constexpr std::uint64_t kInspectorNestedGdtfPreviewBytes =
    16U * 1024U * 1024U;
inline constexpr std::uint64_t kInspectorNestedGdtfOpenBytes =
    512U * 1024U * 1024U;
inline constexpr std::uint64_t kInspectorTextPreviewBytes = 4U * 1024U * 1024U;
inline constexpr std::uint64_t kInspectorEagerXmlBytes = 2U * 1024U * 1024U;
inline constexpr std::size_t kInspectorImageCacheBytes = 64U * 1024U * 1024U;

// Describes whether and how one selected resource may be previewed safely.
struct PreviewPolicyDecision {
  bool allowed = false;
  std::uint64_t maxBytes = 0;
  std::string status;
};

// Applies named automatic-preview limits without changing semantic validity.
PreviewPolicyDecision DecidePreview(
    const perastage::inspection::ResourceDescriptor &resource);

// Applies the hard explicit-open limit to known and unknown nested GDTFs.
PreviewPolicyDecision DecideNestedGdtfOpen(bool sizeKnown,
                                           std::uint64_t size);

// Maps supported archive spelling to a controlled ASCII temporary filename.
std::optional<std::string> InspectorPreviewFilename(
    const std::string &archivePath);

} // namespace gui::inspection
