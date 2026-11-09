/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include <wx/splitter.h>
// The AUI pane retains its legacy name; only its content gains a local splitter.
class ViewportPatchWorkspace : public wxSplitterWindow {
public:
  explicit ViewportPatchWorkspace(wxWindow *parent);
  void SetViewport(wxWindow *viewport);
  void AttachPatch(wxWindow *patch, int height);
  void DetachPatch();
  int PatchHeight() const;
};
