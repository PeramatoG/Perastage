/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/dmx_patch_canvas.h"
#include "patch/viewport_patch_workspace.h"
#include <cassert>
#include <wx/app.h>
#include <wx/aui/aui.h>
#include <wx/frame.h>
#include <wx/sizer.h>
class PatchTestApp final : public wxApp { public: bool OnInit() override { return true; } };
wxIMPLEMENT_APP_NO_MAIN(PatchTestApp);
void Flush() { wxTheApp->ProcessPendingEvents(); wxYield(); }
int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp->CallOnInit());
  auto *frame = new wxFrame(nullptr, wxID_ANY, "DMX Patch smoke", wxDefaultPosition, wxSize(1200, 800));
  wxAuiManager manager(frame);
  auto *three = new ViewportPatchWorkspace(frame);
  auto *two = new ViewportPatchWorkspace(frame);
  auto *threeView = new wxPanel(three); three->SetViewport(threeView);
  auto *twoView = new wxPanel(two); two->SetViewport(twoView);
  manager.AddPane(three, wxAuiPaneInfo().Name("3DViewport").Center().MinSize(200, 180));
  manager.AddPane(two, wxAuiPaneInfo().Name("2DViewport").Center().Hide());
  auto *sidebar = new wxPanel(frame);
  manager.AddPane(sidebar, wxAuiPaneInfo().Name("LayerPanel").Right().BestSize(200, 400));
  const auto legacyPerspective = manager.SavePerspective();
  manager.Update(); frame->Show(); Flush();
  auto *patch = new DmxPatchCanvas(frame);
  auto snapshot = std::make_shared<dmx_patch::Snapshot>();
  snapshot->universe = 70000;
  snapshot->fixtures["a"] = {"Long fixture name continuing across rows", "1", "Stage", "#224488"};
  snapshot->fixtures["b"] = {"Overlap", "2", {}, "#CCEEAA"};
  for (int i = 0; i < 180; ++i) snapshot->cells[i].occupants.push_back({"a", 1, i + 1, 0, "Red", "Red"});
  snapshot->cells[0].occupants.push_back({"b", 1, 1, 0, "Dimmer", "Intensity"});
  patch->SetSnapshot(snapshot);
  three->AttachPatch(patch, frame->FromDIP(240)); Flush();
  assert(three->HasPatch() && patch->GetParent() == three);
  assert(patch->GetSize().x < frame->GetClientSize().x);
  int width = patch->CellSize().x, height = patch->CellSize().y;
  const int originalColumns = patch->ColumnCount();
  assert(width < patch->FromDIP(68));
  assert(patch->AddressAt(wxPoint(1, 1))->channel == 1);
  assert(patch->AddressAt(wxPoint(width + 1, height + 1))->channel == originalColumns + 2);
  frame->SetSize(800, 800); Flush();
  assert(patch->ColumnCount() < originalColumns);
  width = patch->CellSize().x; height = patch->CellSize().y;
  assert(patch->AddressAt(wxPoint(width + 1, height + 1))->channel == patch->ColumnCount() + 2);
  patch->Scroll(0, 8); Flush();
  const auto top = patch->CalcUnscrolledPosition(wxPoint(1, 1));
  assert(patch->AddressAt(wxPoint(1, 1))->channel == (top.y / height) * patch->ColumnCount() + 1);
  // Last partial row must not expose semantic channels beyond 512.
  dmx_patch_ui::GridGeometry layout{13, 52, 46};
  assert(layout.Hit(0, 39 * 46) == 508);
  assert(!layout.Hit(5 * 52, 39 * 46));
  auto &local = three->PaneManager();
  auto &pane = local.GetPane("DmxPatch");
  assert(pane.HasCloseButton() && pane.HasMaximizeButton());
  local.MaximizePane(pane); local.Update(); Flush();
  assert(pane.IsMaximized() && !threeView->IsShownOnScreen());
  local.RestorePane(pane); local.Update(); Flush();
  assert(threeView->IsShownOnScreen());
  three->SetPatchHeight(frame->FromDIP(180)); Flush();
  assert(three->PatchHeight() >= frame->FromDIP(100));
  three->DetachPatch(); patch->Hide(); patch->Reparent(frame); Flush();
  assert(!three->HasPatch());
  assert(threeView->IsShownOnScreen() && threeView->GetSize().y > frame->FromDIP(300));
  manager.GetPane("3DViewport").Hide(); manager.GetPane("2DViewport").Show(); manager.Update();
  two->AttachPatch(patch, frame->FromDIP(180)); Flush();
  assert(two->HasPatch() && patch->GetParent() == two);
  // Old AUI perspectives still address the same panes, with no new global dock.
  two->DetachPatch(); patch->Hide(); patch->Reparent(frame);
  assert(manager.LoadPerspective(legacyPerspective)); Flush();
  assert(manager.GetPane("3DViewport").IsShown() && !manager.GetPane("2DViewport").IsShown());
  three->AttachPatch(patch, frame->FromDIP(180)); Flush();
  assert(three->HasPatch());
  wxAuiManagerEvent close(wxEVT_AUI_PANE_CLOSE);
  close.SetManager(&three->PaneManager()); close.SetPane(&three->PaneManager().GetPane("DmxPatch"));
  three->GetEventHandler()->ProcessEvent(close); Flush();
  assert(!three->HasPatch());
  manager.UnInit(); frame->Destroy(); Flush();
  wxTheApp->OnExit(); wxEntryCleanup();
}
