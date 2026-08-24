#pragma once

class wxWindow;

namespace gui::sceneviewrefresh {

enum class SceneUpdateScope {
  Full,
  Light
};

// Refreshes the 2D, 3D, and layout previews after scene data changes.
void RefreshSceneViewsAfterTableEdit(wxWindow *source, SceneUpdateScope scope);

// Invalidates Layout 2D previews after a confirmed visual scene mutation.
void RefreshLayout2DViewsAfterSceneChange(wxWindow *source);

} // namespace gui::sceneviewrefresh
