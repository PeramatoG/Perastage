/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/viewport_patch_workspace.h"
#include <algorithm>
ViewportPatchWorkspace::ViewportPatchWorkspace(wxWindow *parent)
    : wxSplitterWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3D) {
  SetMinimumPaneSize(FromDIP(80));
  SetSashGravity(1.0);
  Bind(wxEVT_SPLITTER_DOUBLECLICKED, [](wxSplitterEvent &e) { e.Veto(); });
}
void ViewportPatchWorkspace::SetViewport(wxWindow *viewport) {
  viewport->SetMinSize(FromDIP(wxSize(100, 80)));
  Initialize(viewport);
}
void ViewportPatchWorkspace::AttachPatch(wxWindow *patch, int height) {
  if (IsSplit() && GetWindow2() == patch) return;
  patch->Reparent(this); patch->Show();
  // Negative sash positions preserve the requested bottom-panel height.
  SplitHorizontally(GetWindow1(), patch, -std::max(FromDIP(100), height));
  CallAfter([this, patch, height] {
    if (IsSplit() && GetWindow2() == patch)
      SetSashPosition(-std::max(FromDIP(100), height));
  });
}
void ViewportPatchWorkspace::DetachPatch() {
  if (IsSplit()) Unsplit(GetWindow2());
}
int ViewportPatchWorkspace::PatchHeight() const {
  return IsSplit() ? GetWindow2()->GetSize().y : 0;
}
