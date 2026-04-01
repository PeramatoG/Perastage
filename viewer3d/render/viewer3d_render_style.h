#pragma once

#include <string_view>

class ConfigManager;

enum class Viewer3DRenderStyle {
  Standard,
  WhiteModel,
  Textured,
  Wireframe,
  ByDeviceType,
  ByLayer,
  ByUniverse
};

Viewer3DRenderStyle ResolveViewer3DRenderStyle(const ConfigManager &cfg);
const char *ToConfigValue(Viewer3DRenderStyle style);
bool IsWhiteModelRenderStyle(Viewer3DRenderStyle style);
bool IsTexturedRenderStyle(Viewer3DRenderStyle style);
