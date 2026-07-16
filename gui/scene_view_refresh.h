#pragma once

class wxWindow;

namespace gui::sceneviewrefresh {

enum class SceneUpdateScope {
  Full,
  Light
};

// Refreshes the 2D, 3D, and layout previews after scene data changes.
void RefreshSceneViewsAfterTableEdit(wxWindow *source, SceneUpdateScope scope);

} // namespace gui::sceneviewrefresh
