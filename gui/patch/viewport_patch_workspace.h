/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include <wx/panel.h>
#include <wx/aui/aui.h>
#include <functional>
// Retain the outer legacy AUI pane, with native patch controls in its own column.
class ViewportPatchWorkspace : public wxPanel {
public:
  explicit ViewportPatchWorkspace(wxWindow *parent,
      std::function<void()> close = {}, std::function<void(int)> resized = {});
  ~ViewportPatchWorkspace() override;
  void SetViewport(wxWindow *viewport);
  void AttachPatch(wxWindow *patch, int height);
  void DetachPatch();
  bool HasPatch() const;
  int PatchHeight() const;
  void SetPatchHeight(int height);
  wxAuiManager &PaneManager() { return manager_; }
private:
  void OnPatchSize(wxSizeEvent &event);
  wxAuiManager manager_;
  wxWindow *patch_ = nullptr;
  std::function<void()> close_;
  std::function<void(int)> resized_;
  int pendingHeight_ = 0;
  bool updating_ = false, resizeQueued_ = false;
};
