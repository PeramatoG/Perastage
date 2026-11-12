#pragma once
#include <wx/event.h>
#include <wx/string.h>
#include "viewer2doffscreenrenderer.h"
class wxStatusBar;

class MainWindow : public wxEvtHandler {
public:
  Viewer2DOffscreenRenderer renderer;
  bool deferred = false;
  int refreshes = 0;
  bool ShouldDeferAutomaticSceneWork() const { return deferred; }
  Viewer2DOffscreenRenderer *GetOffscreenRenderer() { return &renderer; }
  wxStatusBar *GetStatusBar() { return nullptr; }
  void SetStatusText(const wxString &, int) {}
  void RefreshAfterFixtureSymbolUpdate() { ++refreshes; }
};
