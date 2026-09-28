#include "inspection/inspector_preview_policy.h"

namespace gui::inspection {

// Applies the bounded policy for selection-driven Inspector previews.
PreviewPolicyDecision DecidePreview(
    const perastage::inspection::ResourceDescriptor &resource) {
  using perastage::inspection::ResourceKind;
  std::uint64_t limit = 0;
  switch (resource.kind) {
  case ResourceKind::Image: limit = kInspectorImagePreviewBytes; break;
  case ResourceKind::Model: limit = kInspectorModelPreviewBytes; break;
  case ResourceKind::NestedGdtf: limit = kInspectorNestedGdtfBytes; break;
  case ResourceKind::XmlText:
  case ResourceKind::Text: limit = kInspectorTextPreviewBytes; break;
  case ResourceKind::Binary:
    return {false, 0, "No automatic preview is available for this binary resource."};
  }
  if (!resource.rawReadSupported)
    return {false, limit, "This package entry cannot be read safely."};
  if (resource.sizeKnown && resource.size > limit)
    return {false, limit,
            "The resource remains available but exceeds the automatic preview limit."};
  return {true, limit, "Loading bounded preview..."};
}

} // namespace gui::inspection
