#include "scene_view_refresh.h"

#include "layoutviewerpanel.h"
#include "mainwindow.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

#include <wx/window.h>

namespace gui::sceneviewrefresh {
namespace {

// Finds the first layout viewer in the provided window subtree.
LayoutViewerPanel *FindLayoutViewerPanel(wxWindow *root) {
  if (!root)
    return nullptr;

  if (auto *layoutViewer = dynamic_cast<LayoutViewerPanel *>(root))
    return layoutViewer;

  for (wxWindow *child : root->GetChildren()) {
    if (auto *layoutViewer = FindLayoutViewerPanel(child))
      return layoutViewer;
  }
  return nullptr;
}

// Refreshes the standalone 2D panel and invalidates symbol caches when needed.
void RefreshViewer2D(SceneUpdateScope scope) {
  if (!Viewer2DPanel::Instance())
    return;

  if (scope == SceneUpdateScope::Full) {
    Viewer2DPanel::Instance()->InvalidateBottomSymbolCache();
    Viewer2DPanel::Instance()->UpdateScene();
  } else {
    Viewer2DPanel::Instance()->UpdateScene(false);
  }
  Viewer2DPanel::Instance()->Refresh();
}

// Refreshes the standalone 3D panel and rebuilds its scene when needed.
void RefreshViewer3D(SceneUpdateScope scope) {
  if (!Viewer3DPanel::Instance())
    return;

  if (scope == SceneUpdateScope::Full)
    Viewer3DPanel::Instance()->UpdateScene();
  Viewer3DPanel::Instance()->Refresh();
}

// Refreshes layout-viewer rasters that depend on the edited scene content.
void RefreshLayoutViewer(wxWindow *source) {
  wxWindow *topLevel = wxGetTopLevelParent(source);
  if (auto *mainWindow = dynamic_cast<MainWindow *>(topLevel)) {
    mainWindow->NotifySceneVisualContentChanged();
    return;
  }
  if (auto *layoutViewer = FindLayoutViewerPanel(topLevel))
    layoutViewer->RefreshAfterSceneContentUpdate();
}

} // namespace

// Refreshes all scene viewers after table edits, including layer visibility transitions.
void RefreshSceneViewsAfterTableEdit(wxWindow *source, SceneUpdateScope scope) {
  RefreshViewer2D(scope);
  RefreshViewer3D(scope);
  RefreshLayoutViewer(source);
}

} // namespace gui::sceneviewrefresh
