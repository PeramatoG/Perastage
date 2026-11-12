/* This file is part of Perastage. License: GPL-3.0-or-later. */
// Run against the real application objects with run_viewport_regression_smoke.py --source.
#include <wx/wx.h>
#include <wx/apptrait.h>
#include <wx/stdpaths.h>
#include <wx/aui/aui.h>
#include <wx/dcscreen.h>
#include <wx/dcmemory.h>
#include <wx/bmpbuttn.h>
#include <wx/checkbox.h>
#include <wx/popupwin.h>
#include <wx/listctrl.h>
#include "configmanager.h"
#include "platform_locale.h"
#include "mainwindow.h"
#include "mainwindow/ids/mainwindow_command_ids.h"
#include "patch/dmx_patch_panel.h"
#include "patch/viewport_patch_workspace.h"
#include "startup_profile.h"
#include "viewer3dpanel.h"
#include "viewer2dpanel.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_channel_footprint_cases.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace {
std::filesystem::path scratch;
void Require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
class SmokePaths : public wxStandardPaths {
public: wxString GetUserDataDir() const override { return scratch.string(); }
};
class SmokeTraits : public wxGUIAppTraits {
  SmokePaths paths_;
public: wxStandardPaths &GetStandardPaths() override { return paths_; }
};
DmxPatchPanel *FindPatch(wxWindow &window) {
  for (auto *child : window.GetChildren()) {
    if (auto *panel = dynamic_cast<DmxPatchPanel *>(child)) return panel;
    if (auto *panel = FindPatch(*child)) return panel;
  }
  return nullptr;
}
}
class PatchWorkspaceSmoke : public wxApp {
  MainWindow *frame_ = nullptr;
  wxTimer timer_;
  int stage_ = 0, ticks_ = 0, outcome_ = 1;
  DmxPatchPanel *patch_ = nullptr;
  bool inTick_ = false;
  std::string legacy_;
  std::shared_ptr<startup::Metrics> metrics_ = std::make_shared<startup::Metrics>();
  void Menu(int id) { wxCommandEvent e(wxEVT_MENU, id); frame_->GetEventHandler()->ProcessEvent(e); }
  void Capture(const char *name) {
    timer_.Stop(); frame_->Refresh(); wxYield(); frame_->Update();
    wxMilliSleep(60); wxYield();
    wxScreenDC screen; wxBitmap image(frame_->GetSize().x, frame_->GetSize().y);
    wxMemoryDC memory(image); const auto point = frame_->GetScreenPosition();
    memory.Blit(0, 0, image.GetWidth(), image.GetHeight(), &screen, point.x, point.y);
    memory.SelectObject(wxNullBitmap); image.SaveFile((scratch / name).string(), wxBITMAP_TYPE_PNG);
    timer_.Start(150);
  }
public:
  wxAppTraits *CreateTraits() override { return new SmokeTraits; }
  bool OnInit() override {
    platform::EnsureProcessTextLocale();
    scratch = std::filesystem::absolute(argv[1].ToStdString());
    std::filesystem::create_directories(scratch);
    SetAppName("Perastage"); wxInitAllImageHandlers();
    auto &cfg = ConfigManager::Get(); cfg.Reset();
    if (argc >= 3) cfg.LoadUserConfig();
    cfg.SetValue("ui_dmx_patch_visible", "0");
    cfg.SetValue("layout_view_mode", "2d_layout_view");
    Require(cfg.SaveProject((scratch / "scene.pstg").string()), "Save fixture failed");
    frame_ = new MainWindow("DMX Patch workspace smoke", nullptr, metrics_); SetTopWindow(frame_);
    frame_->LoadStartupProjectFromPath((scratch / "scene.pstg").string());
    timer_.SetOwner(this); Bind(wxEVT_TIMER, &PatchWorkspaceSmoke::Tick, this); timer_.Start(150);
    return true;
  }
  int OnRun() override { wxApp::OnRun(); return outcome_; }
  void Tick(wxTimerEvent &) {
    if (inTick_) return;
    inTick_ = true;
    struct TickGuard { bool &flag; ~TickGuard() { flag = false; } } guard{inTick_};
    try {
      Require(++ticks_ < 400, "Workspace smoke timed out");
      if (frame_->IsStartupInitializationPending()) return;
      auto &cfg = ConfigManager::Get(); auto *manager = wxAuiManager::GetManager(frame_);
      Require(manager, "AUI unavailable");
      std::cout << "Patch workspace stage " << stage_ << std::endl;
      switch (stage_++) {
      case 0:
        Require(!FindPatch(*frame_), "Hidden patch was eagerly constructed");
        Require(metrics_->viewer2DConstructions == 1 && metrics_->viewer3DConstructions == 0, "Lazy viewport startup regressed");
        legacy_ = manager->SavePerspective().ToStdString();
        {
          std::string channels;
          for (int i = 1; i <= 180; ++i) {
            const std::string attribute = i % 3 == 1 ? "Red" : i % 3 == 2 ? "Green" : "Blue";
            channels += "<DMXChannel Geometry='Body' Offset='" + std::to_string(i) +
                "'><LogicalChannel Attribute='" + attribute + "'><ChannelFunction Name='" + attribute +
                "' Attribute='" + attribute + "'/></LogicalChannel></DMXChannel>";
          }
          std::string error;
          Require(tests::archive::WriteStoredZipWithRawNames(scratch / "fixture.gdtf",
              {{"description.xml", tests::gdtf::Description("<Geometry Name='Body'/>", channels)}}, error), "Sample definition failed");
          Fixture fixture;
          fixture.uuid = "11111111-1111-4111-8111-111111111111";
          fixture.instanceName = "RGB pixel strip"; fixture.gdtfMode = "Pixels";
          fixture.gdtfSpec = (scratch / "fixture.gdtf").string();
          fixture.address = "1.1"; fixture.visualColorHex = "#24658C";
          cfg.GetScene().fixtures[fixture.uuid] = fixture;
          fixture.uuid = "22222222-2222-4222-8222-222222222222";
          fixture.instanceName = "Overlapping strip"; fixture.address = "1.7"; fixture.visualColorHex = "#C9D989";
          cfg.GetScene().fixtures[fixture.uuid] = fixture;
          cfg.NotifyChange(ConfigManager::Change::Scene);
        }
        cfg.MarkSaved(); Menu(ID_View_ToggleDmxPatch); break;
      case 1: {
        patch_ = FindPatch(*frame_); Require(patch_ && patch_->IsShownOnScreen(), "Patch not visible");
        auto *workspace = dynamic_cast<ViewportPatchWorkspace *>(patch_->GetParent());
        Require(workspace && workspace->HasPatch(), "No local patch pane");
        Require(patch_->GetSize().x == workspace->GetSize().x && patch_->GetSize().x < frame_->GetSize().x, "Patch spans other columns");
        Require(patch_->GetSize().y >= frame_->FromDIP(100), "Patch height impractical");
        bool hasStatus = false;
        for (auto *child : patch_->GetChildren())
          if (auto *text = dynamic_cast<wxStaticText *>(child); text && text->GetLabel().Contains("/512")) {
            hasStatus = text->IsShownOnScreen() && text->GetSize().y >= text->GetCharHeight();
            std::cout << "Patch status: " << text->GetLabel() << " size=" << text->GetSize().x << ',' << text->GetSize().y << std::endl;
          }
        Require(hasStatus, "Occupied channel status is missing or clipped");
        wxCheckBox *compatibility = nullptr;
        for (auto *child : patch_->GetChildren())
          if (auto *checkbox = dynamic_cast<wxCheckBox *>(child)) compatibility = checkbox;
        Require(compatibility && !compatibility->GetValue(), "Compatibility must require explicit opt-in");
        compatibility->SetValue(true);
        wxCommandEvent preview(wxEVT_CHECKBOX, compatibility->GetId());
        compatibility->GetEventHandler()->ProcessEvent(preview);
        bool provisionalCounter = false;
        for (auto *child : patch_->GetChildren())
          if (auto *text = dynamic_cast<wxStaticText *>(child))
            provisionalCounter |= text->GetLabel().Contains("Provisional:");
        Require(provisionalCounter && !cfg.IsDirty(), "Preview is not explicit/read-only");
        compatibility->SetValue(false); compatibility->GetEventHandler()->ProcessEvent(preview);
        workspace->SetPatchHeight(frame_->FromDIP(180));
        wxYield();
        Require(!cfg.IsDirty(), "Viewing/resizing patch dirtied project");
        Require(cfg.SaveUserConfig(), "Patch preferences save failed");
        UserPreferencesStore restored;
        Require(restored.LoadUserConfig() && restored.GetValue("ui_dmx_patch_visible") == "1" &&
            restored.GetValue("ui_dmx_patch_height") == cfg.GetValue("ui_dmx_patch_height"), "Patch preferences did not round-trip");
        Capture("dmx-patch-2d.png");
        wxBitmapButton *warnings = nullptr;
        for (auto *child : patch_->GetChildren())
          if (auto *button = dynamic_cast<wxBitmapButton *>(child)) warnings = button;
        Require(warnings && warnings->IsEnabled(), "Conflict diagnostics button unavailable");
        wxCommandEvent showWarnings(wxEVT_BUTTON, warnings->GetId());
        warnings->GetEventHandler()->ProcessEvent(showWarnings); wxYield();
        wxPopupTransientWindow *popup = nullptr;
        for (auto *child : patch_->GetChildren())
          if (auto *candidate = dynamic_cast<wxPopupTransientWindow *>(child)) popup = candidate;
        Require(popup && popup->IsShownOnScreen(), "Diagnostics dropdown did not open");
        wxListView *list = nullptr;
        for (auto *child : popup->GetChildren())
          if (auto *candidate = dynamic_cast<wxListView *>(child)) list = candidate;
        Require(list && list->GetItemCount() == 2, "Conflict ranges were not grouped by fixture");
        Capture("dmx-patch-diagnostics.png");
        popup->Dismiss(); popup->Destroy(); wxYield();
        Menu(ID_View_Layout_Default); break;
      }
      case 2:
        Require(FindPatch(*frame_) == patch_ && patch_->IsShownOnScreen(), "3D switch lost shared panel");
        Require(patch_->GetParent() == manager->GetPane("3DViewport").window, "Patch not under 3D");
        Require(manager->GetPane("3DViewport").IsShown() && !manager->GetPane("2DViewport").IsShown(), "3D pane visibility is wrong");
        Require(Viewer3DPanel::Instance()->IsShownOnScreen() && !Viewer2DPanel::Instance()->IsShownOnScreen(), "Viewport native visibility is wrong");
        Capture("dmx-patch-3d.png"); Menu(ID_View_Layout_Mode); break;
      case 3:
        Require(!patch_->IsShownOnScreen(), "Layout mode did not hide patch");
        Menu(ID_View_Layout_2D); break;
      case 4:
        Require(patch_->IsShownOnScreen(), "Returning from Layout did not restore patch");
        Menu(ID_View_Layout_Inspector); break;
      case 5:
        Require(!patch_->IsShownOnScreen(), "Inspector did not hide patch");
        Menu(ID_View_Layout_2D); break;
      case 6:
        Require(patch_->IsShownOnScreen(), "Returning from Inspector lost patch");
        Require(manager->LoadPerspective(legacy_), "Legacy perspective failed to load");
        cfg.MarkSaved(); // Layout presets have their own existing preference writes.
        {
          auto *workspace = dynamic_cast<ViewportPatchWorkspace *>(patch_->GetParent());
          wxAuiManagerEvent close(wxEVT_AUI_PANE_CLOSE);
          close.SetManager(&workspace->PaneManager()); close.SetPane(&workspace->PaneManager().GetPane("DmxPatch"));
          workspace->GetEventHandler()->ProcessEvent(close);
        }
        break;
      case 7:
        Require(!patch_->IsShownOnScreen(), "Closing patch failed");
        Require(!dynamic_cast<ViewportPatchWorkspace *>(manager->GetPane("2DViewport").window)->HasPatch(), "Viewport space not reclaimed");
        Require(!cfg.IsDirty(), "Closing patch changed project dirty state");
        // This smoke creates its sample after project loading; leave close clean.
        cfg.GetScene().fixtures.clear();
        Require(cfg.SaveUserConfig(), "Preferences save failed");
        std::cout << "DMX Patch actual workspace checks passed" << std::endl;
        outcome_ = 0; timer_.Stop(); frame_->Destroy(); break;
      }
    } catch (const std::exception &error) {
      std::cerr << error.what() << std::endl; timer_.Stop(); outcome_ = 1; frame_->Destroy();
    }
  }
};
wxIMPLEMENT_APP(PatchWorkspaceSmoke);
