#include "viewer3d_render_style.h"

#include "configmanager.h"

namespace {
constexpr std::string_view kStandardStyleValue = "standard";
constexpr std::string_view kWhiteStyleValue = "white";
constexpr std::string_view kWhiteModelStyleValue = "white_model";
constexpr std::string_view kTexturedStyleValue = "textured";
constexpr std::string_view kWireframeStyleValue = "wireframe";
constexpr std::string_view kByDeviceTypeStyleValue = "by_device_type";
constexpr std::string_view kByLayerStyleValue = "by_layer";
constexpr std::string_view kByUniverseStyleValue = "by_universe";
} // namespace

Viewer3DRenderStyle ResolveViewer3DRenderStyle(const ConfigManager &cfg) {
  const auto style = cfg.GetValue("viewer3d_render_style");
  if (!style.has_value())
    return Viewer3DRenderStyle::Standard;

  if (*style == kWhiteModelStyleValue)
    return Viewer3DRenderStyle::WhiteModel;
  if (*style == kWhiteStyleValue)
    return Viewer3DRenderStyle::White;
  if (*style == kTexturedStyleValue)
    return Viewer3DRenderStyle::Textured;
  if (*style == kWireframeStyleValue)
    return Viewer3DRenderStyle::Wireframe;
  if (*style == kByDeviceTypeStyleValue)
    return Viewer3DRenderStyle::ByDeviceType;
  if (*style == kByLayerStyleValue)
    return Viewer3DRenderStyle::ByLayer;
  if (*style == kByUniverseStyleValue)
    return Viewer3DRenderStyle::ByUniverse;
  return Viewer3DRenderStyle::Standard;
}

const char *ToConfigValue(Viewer3DRenderStyle style) {
  switch (style) {
  case Viewer3DRenderStyle::White:
    return kWhiteStyleValue.data();
  case Viewer3DRenderStyle::WhiteModel:
    return kWhiteModelStyleValue.data();
  case Viewer3DRenderStyle::Textured:
    return kTexturedStyleValue.data();
  case Viewer3DRenderStyle::Wireframe:
    return kWireframeStyleValue.data();
  case Viewer3DRenderStyle::ByDeviceType:
    return kByDeviceTypeStyleValue.data();
  case Viewer3DRenderStyle::ByLayer:
    return kByLayerStyleValue.data();
  case Viewer3DRenderStyle::ByUniverse:
    return kByUniverseStyleValue.data();
  case Viewer3DRenderStyle::Standard:
  default:
    return kStandardStyleValue.data();
  }
}

bool IsWhiteModelRenderStyle(Viewer3DRenderStyle style) {
  return style == Viewer3DRenderStyle::WhiteModel ||
         style == Viewer3DRenderStyle::White ||
         style == Viewer3DRenderStyle::ByDeviceType ||
         style == Viewer3DRenderStyle::ByLayer ||
         style == Viewer3DRenderStyle::ByUniverse;
}

bool IsTexturedRenderStyle(Viewer3DRenderStyle style) {
  return style == Viewer3DRenderStyle::Textured;
}
