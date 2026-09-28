#include "inspection/inspector_preview_policy.h"

#include <algorithm>
#include <cctype>

namespace gui::inspection {

// Applies the bounded policy for selection-driven Inspector previews.
PreviewPolicyDecision DecidePreview(
    const perastage::inspection::ResourceDescriptor &resource) {
  using perastage::inspection::ResourceKind;
  std::uint64_t limit = 0;
  switch (resource.kind) {
  case ResourceKind::Image: limit = kInspectorImagePreviewBytes; break;
  case ResourceKind::Model: limit = kInspectorModelPreviewBytes; break;
  case ResourceKind::NestedGdtf:
    return {false, 0,
            "Activate this embedded GDTF to inspect it."};
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

// Keeps unknown reads bounded and rejects known resources above the hard cap.
PreviewPolicyDecision DecideNestedGdtfOpen(bool sizeKnown,
                                           std::uint64_t size) {
  if (sizeKnown && size > kInspectorNestedGdtfOpenBytes)
    return {false, kInspectorNestedGdtfOpenBytes,
            "This embedded GDTF exceeds the Inspector open limit."};
  return {true, kInspectorNestedGdtfOpenBytes,
          "Opening bounded embedded GDTF..."};
}

// Selects only controlled filenames for explicitly supported preview types.
std::optional<std::string> InspectorPreviewFilename(
    const std::string &archivePath) {
  const auto separator = archivePath.find_last_of("/\\");
  const auto dot = archivePath.find_last_of('.');
  if (dot == std::string::npos ||
      (separator != std::string::npos && dot < separator))
    return std::nullopt;
  std::string extension = archivePath.substr(dot);
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });
  if (extension == ".glb") return "preview.glb";
  if (extension == ".3ds") return "preview.3ds";
  if (extension == ".gdtf") return "preview.gdtf";
  return std::nullopt;
}

} // namespace gui::inspection
