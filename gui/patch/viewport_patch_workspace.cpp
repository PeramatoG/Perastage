/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/viewport_patch_workspace.h"
#include <algorithm>
ViewportPatchWorkspace::ViewportPatchWorkspace(wxWindow *parent,
    std::function<void()> close, std::function<void(int)> resized)
    : wxPanel(parent), manager_(this), close_(std::move(close)), resized_(std::move(resized)) {
  manager_.SetDockSizeConstraint(1.0, 1.0);
  Bind(wxEVT_AUI_PANE_CLOSE, [this](wxAuiManagerEvent &event) {
    if (event.GetManager() != &manager_ || !event.GetPane() || event.GetPane()->name != "DmxPatch") {
      event.Skip(); return;
    }
    event.Veto();
    CallAfter([this] { if (close_) close_(); else DetachPatch(); });
  });
}
ViewportPatchWorkspace::~ViewportPatchWorkspace() {
  if (patch_) patch_->Unbind(wxEVT_SIZE, &ViewportPatchWorkspace::OnPatchSize, this);
  manager_.UnInit();
}
void ViewportPatchWorkspace::SetViewport(wxWindow *viewport) {
  viewport->SetMinSize(FromDIP(wxSize(100, 80)));
  manager_.AddPane(viewport, wxAuiPaneInfo().Name("SceneViewport").CenterPane().PaneBorder(false));
  manager_.Update();
}
void ViewportPatchWorkspace::AttachPatch(wxWindow *patch, int height) {
  if (patch_ == patch) return;
  updating_ = true;
  patch_ = patch;
  patch->Reparent(this);
  manager_.AddPane(patch, wxAuiPaneInfo().Name("DmxPatch").Caption(_("DMX Patch"))
      .Bottom().Layer(0).BestSize(-1, std::max(FromDIP(100), height))
      .MinSize(FromDIP(wxSize(120, 80))).Movable(false).Dockable(false).Floatable(false)
      .CloseButton(true).MaximizeButton(true).PaneBorder(false));
  patch->Bind(wxEVT_SIZE, &ViewportPatchWorkspace::OnPatchSize, this);
  manager_.Update();
  updating_ = false;
}
void ViewportPatchWorkspace::DetachPatch() {
  if (!patch_) return;
  updating_ = true;
  auto &pane = manager_.GetPane(patch_);
  if (pane.IsMaximized()) manager_.RestorePane(pane);
  patch_->Unbind(wxEVT_SIZE, &ViewportPatchWorkspace::OnPatchSize, this);
  manager_.DetachPane(patch_);
  patch_->Hide(); patch_ = nullptr;
  manager_.Update();
  updating_ = false;
}
bool ViewportPatchWorkspace::HasPatch() const { return patch_ != nullptr; }
int ViewportPatchWorkspace::PatchHeight() const { return patch_ ? patch_->GetSize().y : 0; }
void ViewportPatchWorkspace::SetPatchHeight(int height) {
  if (!patch_) return;
  auto pane = manager_.GetPane(patch_);
  if (pane.IsMaximized()) { manager_.RestorePane(manager_.GetPane(patch_)); pane = manager_.GetPane(patch_); }
  pane.BestSize(-1, std::max(FromDIP(100), height));
  // Rebuild the bottom dock from the requested best size; viewport remains central.
  auto *patch = patch_;
  updating_ = true;
  manager_.DetachPane(patch); manager_.Update();
  manager_.AddPane(patch, pane); manager_.Update();
  updating_ = false;
}
void ViewportPatchWorkspace::OnPatchSize(wxSizeEvent &event) {
  event.Skip();
  if (updating_ || resizeQueued_) return;
  resizeQueued_ = true;
  CallAfter([this] {
    resizeQueued_ = false;
    if (patch_ && !manager_.GetPane(patch_).IsMaximized() && resized_)
      resized_(PatchHeight());
  });
}
