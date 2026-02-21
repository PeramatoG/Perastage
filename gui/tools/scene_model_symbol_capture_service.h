#pragma once

#include <optional>
#include <string>
#include <vector>

#include <wx/gdicmn.h>

#include "symbols/Symbol2D.h"

class ConfigManager;
class Viewer2DOffscreenRenderer;

namespace tools {

enum class SceneModelKind {
  Fixture,
  Truss,
  SceneObject,
};

struct SceneModelSymbolTarget {
  SceneModelKind kind = SceneModelKind::Fixture;
  std::string uuid;
};

struct SceneModelSymbolCaptureOptions {
  bool alignToLocalAxes = false;
  std::optional<std::string> forcedFixtureColor;
  wxSize viewportSize = wxSize(1200, 1200);
};

struct SceneModelSymbolCaptureResult {
  bool ok = false;
  std::string error;
  std::vector<symbols::Symbol2D> symbols;
};

SceneModelSymbolCaptureResult
CaptureSceneModelOrthographicSymbols(Viewer2DOffscreenRenderer &renderer,
                                     ConfigManager &cfg,
                                     const SceneModelSymbolTarget &target,
                                     const SceneModelSymbolCaptureOptions &options = {});

} // namespace tools
