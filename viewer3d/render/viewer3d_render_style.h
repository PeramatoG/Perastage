#pragma once

#include <string_view>
#include <optional>
#include <string>

class ConfigManager;

enum class Viewer3DRenderStyle {
  Standard,
  White,
  WhiteModel,
  Textured,
  Wireframe,
  ByDeviceType,
  ByLayer,
  ByUniverse
};

Viewer3DRenderStyle ParseViewer3DRenderStyle(const std::optional<std::string> &value);
Viewer3DRenderStyle ResolveViewer3DRenderStyle(const ConfigManager &cfg);
const char *ToConfigValue(Viewer3DRenderStyle style);
bool IsWhiteModelRenderStyle(Viewer3DRenderStyle style);
bool IsTexturedRenderStyle(Viewer3DRenderStyle style);
