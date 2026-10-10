#include "viewer3d_render_style.h"
#include "configmanager.h"

Viewer3DRenderStyle ResolveViewer3DRenderStyle(const ConfigManager &cfg) {
  return ParseViewer3DRenderStyle(cfg.GetValue("viewer3d_render_style"));
}
