#pragma once

#include <filesystem>
#include <string>

#include "truss.h"

namespace gui {

enum class PreviewResourceKind { Unsupported, Gdtf, Glb, ThreeDs };

// Classifies a preview resource path by renderable file extension.
PreviewResourceKind GetPreviewResourceKind(const std::string &path);

// Reports whether a preview resource path can be loaded by the preview panel.
bool IsRenderablePreviewResource(const std::string &path);

// Resolves a truss preview resource without mutating the truss representation.
std::string ResolveTrussPreviewResourcePath(const Truss &truss,
                                            const std::string &sceneBasePath);

} // namespace gui
