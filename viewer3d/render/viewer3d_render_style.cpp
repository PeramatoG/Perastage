#include "viewer3d_render_style.h"

#include "configmanager.h"

namespace {
constexpr std::string_view kStandardStyleValue = "standard";
constexpr std::string_view kWhiteModelStyleValue = "white_model";
constexpr std::string_view kTexturedStyleValue = "textured";
} // namespace

Viewer3DRenderStyle ResolveViewer3DRenderStyle(const ConfigManager &cfg) {
  const auto style = cfg.GetValue("viewer3d_render_style");
  if (!style.has_value())
    return Viewer3DRenderStyle::Standard;

  if (*style == kWhiteModelStyleValue)
    return Viewer3DRenderStyle::WhiteModel;
  if (*style == kTexturedStyleValue)
    return Viewer3DRenderStyle::Textured;
  return Viewer3DRenderStyle::Standard;
}

const char *ToConfigValue(Viewer3DRenderStyle style) {
  switch (style) {
  case Viewer3DRenderStyle::WhiteModel:
    return kWhiteModelStyleValue.data();
  case Viewer3DRenderStyle::Textured:
    return kTexturedStyleValue.data();
  case Viewer3DRenderStyle::Standard:
  default:
    return kStandardStyleValue.data();
  }
}

bool IsWhiteModelRenderStyle(Viewer3DRenderStyle style) {
  return style == Viewer3DRenderStyle::WhiteModel;
}

bool IsTexturedRenderStyle(Viewer3DRenderStyle style) {
  return style == Viewer3DRenderStyle::Textured;
}

