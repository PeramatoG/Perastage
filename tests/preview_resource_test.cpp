#include <cassert>
#include <filesystem>

#include "preview_resource.h"

namespace fs = std::filesystem;

// Verifies preview resource extension dispatch and truss source precedence.
int main() {
  assert(gui::GetPreviewResourceKind("fixture.gdtf") ==
         gui::PreviewResourceKind::Gdtf);
  assert(gui::GetPreviewResourceKind("model.GLB") ==
         gui::PreviewResourceKind::Glb);
  assert(gui::GetPreviewResourceKind("model.3ds") ==
         gui::PreviewResourceKind::ThreeDs);
  assert(gui::GetPreviewResourceKind("symbol.svg") ==
         gui::PreviewResourceKind::Unsupported);

  const fs::path sceneBase = fs::path("projects") / "show";
  Truss geometryOnly;
  geometryOnly.symbolFile = "symbols/truss.glb";
  geometryOnly.modelFile = "models/truss.3ds";
  const std::string beforeGdtf = geometryOnly.gdtfSpec;
  assert(fs::path(gui::ResolveTrussPreviewResourcePath(
             geometryOnly, sceneBase.generic_string())) ==
         sceneBase / "symbols" / "truss.glb");
  assert(geometryOnly.gdtfSpec == beforeGdtf);

  Truss gdtfBacked = geometryOnly;
  gdtfBacked.gdtfSpec = "trusses/type.gdtf";
  assert(fs::path(gui::ResolveTrussPreviewResourcePath(
             gdtfBacked, sceneBase.generic_string())) ==
         sceneBase / "trusses" / "type.gdtf");
}
