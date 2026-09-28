#include "inspection/inspector_preview_policy.h"

#include <cassert>

// Verifies explicit preview bounds remain separate from package validity.
int main() {
  using namespace gui::inspection;
  perastage::inspection::ResourceDescriptor image;
  image.kind = perastage::inspection::ResourceKind::Image;
  image.rawReadSupported = true;
  image.sizeKnown = true;
  image.size = kInspectorImagePreviewBytes;
  assert(DecidePreview(image).allowed);
  image.size++;
  const auto oversized = DecidePreview(image);
  assert(!oversized.allowed);
  assert(oversized.maxBytes == kInspectorImagePreviewBytes);

  perastage::inspection::ResourceDescriptor binary;
  binary.rawReadSupported = true;
  assert(!DecidePreview(binary).allowed);
  return 0;
}
