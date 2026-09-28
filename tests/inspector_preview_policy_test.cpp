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

  perastage::inspection::ResourceDescriptor nested;
  nested.kind = perastage::inspection::ResourceKind::NestedGdtf;
  nested.rawReadSupported = true;
  const auto nestedPreview = DecidePreview(nested);
  assert(nestedPreview.allowed);
  assert(nestedPreview.maxBytes == kInspectorNestedGdtfPreviewBytes);
  nested.sizeKnown = true;
  nested.size = kInspectorNestedGdtfPreviewBytes + 1;
  assert(!DecidePreview(nested).allowed);

  assert(DecideNestedGdtfOpen(true, kInspectorNestedGdtfOpenBytes).allowed);
  assert(!DecideNestedGdtfOpen(true,
                               kInspectorNestedGdtfOpenBytes + 1).allowed);
  const auto unknown = DecideNestedGdtfOpen(false, 0);
  assert(unknown.allowed);
  assert(unknown.maxBytes == kInspectorNestedGdtfOpenBytes);
  assert(InspectorPreviewFilename("CON.glb") == "preview.glb");
  assert(InspectorPreviewFilename("bad:name.3DS") == "preview.3ds");
  assert(InspectorPreviewFilename("unicode/照明.gdtf") == "preview.gdtf");
  assert(!InspectorPreviewFilename("archive.exe"));
  return 0;
}
