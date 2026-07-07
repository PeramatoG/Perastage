#include "preview_resource.h"

#include <algorithm>
#include <cctype>

#include "filesystem_path_utils.h"

namespace gui {
namespace {

// Returns the lower-case extension for a UTF-8 file path.
std::string LowerExtension(const std::string &path) {
  std::string ext = PathUtils::PathFromUtf8(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return ext;
}

// Resolves a non-empty resource reference against the scene directory.
std::string ResolveResourcePath(const std::string &reference,
                                const std::filesystem::path &sceneBasePath) {
  if (reference.empty() || !IsRenderablePreviewResource(reference))
    return {};

  std::filesystem::path path = PathUtils::PathFromUtf8(reference);
  if (path.is_relative())
    path = sceneBasePath / path;
  return PathUtils::PathToUtf8(path.lexically_normal());
}

} // namespace

// Classifies a preview resource path by renderable file extension.
PreviewResourceKind GetPreviewResourceKind(const std::string &path) {
  const std::string ext = LowerExtension(path);
  if (ext == ".gdtf")
    return PreviewResourceKind::Gdtf;
  if (ext == ".glb")
    return PreviewResourceKind::Glb;
  if (ext == ".3ds")
    return PreviewResourceKind::ThreeDs;
  return PreviewResourceKind::Unsupported;
}

// Reports whether a preview resource path can be loaded by the preview panel.
bool IsRenderablePreviewResource(const std::string &path) {
  return GetPreviewResourceKind(path) != PreviewResourceKind::Unsupported;
}

// Resolves a truss preview resource without mutating the truss representation.
std::string ResolveTrussPreviewResourcePath(const Truss &truss,
                                            const std::string &sceneBasePath) {
  const std::filesystem::path basePath = PathUtils::PathFromUtf8(sceneBasePath);
  for (const std::string *reference :
       {&truss.gdtfSpec, &truss.symbolFile, &truss.modelFile}) {
    std::string resolved = ResolveResourcePath(*reference, basePath);
    if (!resolved.empty())
      return resolved;
  }
  return {};
}

} // namespace gui
