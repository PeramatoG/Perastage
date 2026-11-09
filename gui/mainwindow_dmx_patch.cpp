/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "mainwindow.h"
#include "guiconfigservices.h"
#include "configmanager.h"
#include "patch/dmx_patch_panel.h"
#include "patch/viewport_patch_workspace.h"
#include <algorithm>

void MainWindow::InitializeDmxPatch() {
  Bind(wxEVT_MENU, [this](wxCommandEvent &) { ToggleDmxPatch(); }, ID_View_ToggleDmxPatch);
}
void MainWindow::SaveDmxPatchPreference(const std::string &key, const std::string &value) {
  auto &config = guiConfigServices->LegacyConfigManager();
  const auto dirty = config.CaptureDirtyState();
  guiConfigServices->Preferences().SetValue(key, value);
  config.RestoreDirtyState(dirty);
}
ViewportPatchWorkspace *MainWindow::CreateDmxViewportWorkspace() {
  auto *workspace = new ViewportPatchWorkspace(this);
  workspace->Bind(wxEVT_SPLITTER_SASH_POS_CHANGED, [this, workspace](wxSplitterEvent &e) {
    if (workspace->IsSplit())
      SaveDmxPatchPreference("ui_dmx_patch_height", std::to_string(ToDIP(workspace->PatchHeight())));
    e.Skip();
  });
  return workspace;
}
void MainWindow::ToggleDmxPatch() {
  auto &preferences = guiConfigServices->Preferences();
  const bool enabled = preferences.GetValue("ui_dmx_patch_visible").value_or("0") == "1";
  SaveDmxPatchPreference("ui_dmx_patch_visible", enabled ? "0" : "1");
  SyncDmxPatchWorkspace();
  UpdateViewMenuChecks();
}
void MainWindow::SyncDmxPatchWorkspace() {
  if (!auiManager || !GetMenuBar()) return;
  auto &two = auiManager->GetPane("2DViewport");
  auto &three = auiManager->GetPane("3DViewport");
  // Legacy saved perspectives may still contain the former 600px minimum.
  if (two.IsOk()) two.MinSize(FromDIP(wxSize(200, 180)));
  if (three.IsOk()) three.MinSize(FromDIP(wxSize(200, 180)));
  auto *target = two.IsOk() && two.IsShown() ? viewport2DWorkspace
      : three.IsOk() && three.IsShown() ? viewport3DWorkspace : nullptr;
  const bool enabled = guiConfigServices->Preferences().GetValue("ui_dmx_patch_visible").value_or("0") == "1";
  const bool active = target && enabled;
  GetMenuBar()->Enable(ID_View_ToggleDmxPatch, target != nullptr);
  GetMenuBar()->Check(ID_View_ToggleDmxPatch, active);
  if (dmxPatchPanel && dmxPatchPanel->GetParent() != target) {
    if (auto *old = dynamic_cast<ViewportPatchWorkspace *>(dmxPatchPanel->GetParent())) old->DetachPatch();
    dmxPatchPanel->Hide(); dmxPatchPanel->Reparent(this);
  }
  if (!active) {
    if (dmxPatchPanel) {
      if (auto *old = dynamic_cast<ViewportPatchWorkspace *>(dmxPatchPanel->GetParent())) old->DetachPatch();
      dmxPatchPanel->Hide(); dmxPatchPanel->Reparent(this);
    }
    return;
  }
  if (!dmxPatchPanel)
    dmxPatchPanel = new DmxPatchPanel(this, guiConfigServices->LegacyConfigManager(), [this] { ToggleDmxPatch(); });
  long height = 240;
  const auto stored = guiConfigServices->Preferences().GetValue("ui_dmx_patch_height");
  if (stored) wxString::FromUTF8(*stored).ToLong(&height);
  target->AttachPatch(dmxPatchPanel, FromDIP(static_cast<int>(std::clamp(height, 100L, 1600L))));
}
