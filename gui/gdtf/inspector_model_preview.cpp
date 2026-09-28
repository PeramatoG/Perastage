#include "gdtf/inspector_model_preview.h"
#include "inspection/inspector_preview_policy.h"

#include "filesystem_path_utils.h"
#include "loader3ds.h"
#include "loaderglb.h"
#include "runtime_storage.h"

#include <fstream>

namespace gui::inspection {

// Materializes bytes privately and parses geometry without texture or GUI work.
std::optional<Mesh> PrepareInspectorModel(
    const std::vector<std::uint8_t> &bytes, const std::string &archivePath,
    std::string *error) {
  const auto filename = InspectorPreviewFilename(archivePath);
  if (!filename || *filename == "preview.gdtf") {
    if (error)
      *error = "Only GLB and 3DS geometry can be prepared in the worker.";
    return {};
  }
  runtime_storage::TemporaryWorkspace workspace("inspector-model-prepare",
                                                 false);
  if (!workspace.IsValid()) {
    if (error)
      *error = "Could not create the temporary model workspace.";
    return {};
  }
  const auto path = workspace.Path() / *filename;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (!output) {
    if (error)
      *error = "Could not write the temporary model resource.";
    return {};
  }

  Mesh mesh;
  const auto nativePath = PathUtils::PathToUtf8(path);
  const bool loaded = *filename == "preview.glb"
                          ? LoadGLB(nativePath, mesh, error, false)
                          : Load3DS(nativePath, mesh,
                                    viewer3d::kApplyThreeDsObjectTransforms,
                                    error, false);
  if (!loaded || mesh.vertices.empty() || mesh.indices.empty())
    return {};
  return mesh;
}

} // namespace gui::inspection
