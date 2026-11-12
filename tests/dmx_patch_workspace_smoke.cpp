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
#include <wx/combobox.h>
#include <wx/uiaction.h>
#include "configmanager.h"
#include "platform_locale.h"
#include "mainwindow.h"
#include "mainwindow/ids/mainwindow_command_ids.h"
#include "patch/dmx_patch_panel.h"
#include "patch/dmx_patch_canvas.h"
#include "fixturetablepanel.h"
#include "fixturetable/fixture_table_highlight_event.h"
#include "selection_movement_settings.h"
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
    cfg.SetValue("ui_dmx_patch_visible", argc >= 3 ? "1" : "0");
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
      // Snapshot publication is coalesced through the GUI event queue; a timer
      // can run before its idle callback on a loaded CI worker.
      if (stage_ == 1 || (stage_ == 0 && argc >= 3)) {
        auto *pending = FindPatch(*frame_);
        bool ready = false;
        if (pending) for (auto *child : pending->GetChildren())
          if (auto *text = dynamic_cast<wxStaticText *>(child)) ready |= text->GetLabel().Contains("/512");
        if (!ready) return;
      }
      auto &cfg = ConfigManager::Get(); auto *manager = wxAuiManager::GetManager(frame_);
      Require(manager, "AUI unavailable");
      std::cout << "Patch workspace stage " << stage_ << std::endl;
      switch (stage_++) {
      case 0:
        if (argc < 3) Require(!FindPatch(*frame_), "Hidden patch was eagerly constructed");
        else {
          auto *initial = FindPatch(*frame_);
          Require(initial && initial->IsShownOnScreen(), "Restored patch did not open on startup");
          bool populated = false;
          for (auto *child : initial->GetChildren())
            if (auto *text = dynamic_cast<wxStaticText *>(child)) populated |= text->GetLabel().Contains("/512");
          Require(populated, "Startup patch was not populated before interaction");
        }
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
        FixtureTablePanel::Instance()->ReloadData();
        cfg.MarkSaved(); if (argc < 3) Menu(ID_View_ToggleDmxPatch); break;
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
        DmxPatchCanvas *canvas = nullptr;
        for (auto *child : patch_->GetChildren())
          if (auto *candidate = dynamic_cast<DmxPatchCanvas *>(child)) canvas = candidate;
        Require(canvas, "Patch canvas unavailable");
        const std::string first = "11111111-1111-4111-8111-111111111111";
        const std::string second = "22222222-2222-4222-8222-222222222222";
        auto *fixtureTable = FixtureTablePanel::Instance();
        Require(fixtureTable, "Fixture table unavailable");
        const auto hoverPatch = [&](int channel) {
          const auto size = canvas->CellSize();
          wxMouseEvent motion(wxEVT_MOTION);
          motion.SetPosition(canvas->CalcScrolledPosition(wxPoint(
              ((channel - 1) % canvas->ColumnCount()) * size.x + size.x / 2,
              ((channel - 1) / canvas->ColumnCount()) * size.y + size.y / 2)));
          canvas->GetEventHandler()->ProcessEvent(motion);
        };
        hoverPatch(1);
        Require(canvas->Highlights().at(first) == gui::FixtureHoverColor() &&
            fixtureTable->GetHighlightedUuid() == first && Viewer2DPanel::Instance()->GetHoveredUuid() == first,
            "Patch mouse motion did not publish green hover to table/viewer");
        auto *tableControl = fixtureTable->GetTableCtrl();
        const auto requireGreenRow = [&] {
          for (unsigned row = 0; row < tableControl->GetItemCount(); ++row)
            if (tableControl->GetTextValue(row, 1) == "RGB pixel strip") {
              wxDataViewItemAttr attr;
              auto *store = dynamic_cast<ColorfulDataViewListStore *>(tableControl->GetModel());
              Require(store && store->GetAttrByRow(row, 1, attr) &&
                  attr.GetBackgroundColour() == gui::FixtureHoverColor(), "Fixture table green styling is missing");
              return row;
            }
          throw std::runtime_error("Hover fixture row missing");
        };
        const unsigned hoverRow = requireGreenRow();
        fixtureTable->ReloadData(); wxYield(); requireGreenRow();
        hoverPatch(187);
        Require(canvas->Highlights().empty() && fixtureTable->GetHighlightedUuid().empty(), "Free channel did not clear green hover");
        const auto itemRect = tableControl->GetItemRect(tableControl->RowToItem(hoverRow), tableControl->GetColumn(1));
        wxMouseEvent tableMotion(wxEVT_MOTION); tableMotion.SetEventObject(tableControl);
        tableMotion.SetPosition(wxPoint(itemRect.x + 4, itemRect.y + itemRect.height / 2));
        tableControl->GetEventHandler()->ProcessEvent(tableMotion);
        Require(canvas->Highlights().at(first) == gui::FixtureHoverColor(), "Table mouse motion did not highlight patch");
        fixtureTable->PreviewFixtureHover({});
        cfg.SetSelectedFixtures({first});
        fixtureTable->HighlightFixture(second, {first, second});
        Require(canvas->Highlights().at(first) == gui::FixtureSelectionColor() &&
            canvas->Highlights().at(second) == gui::FixtureHoverColor(), "Patch selection/hover colors are not synchronized");
        Capture("dmx-patch-highlight.png");
        cfg.SetSelectedFixtures({});
        Require(canvas->Highlights().at(first) == gui::FixtureRelatedHoverColor(), "Related group hover did not replace cleared selection");
        fixtureTable->HighlightFixture({});
        Require(canvas->Highlights().empty() && !cfg.IsDirty(), "Cleared hover remained in patch or changed project");
        const auto pick = [&](int channel, bool shift = false, bool control = false) {
          const auto size = canvas->CellSize();
          const int index = channel - 1;
          const wxPoint point = canvas->CalcScrolledPosition(wxPoint(
              (index % canvas->ColumnCount()) * size.x + size.x / 2,
              (index / canvas->ColumnCount()) * size.y + size.y / 2));
          wxMouseEvent click(wxEVT_LEFT_DOWN); click.SetPosition(point);
          click.SetShiftDown(shift); click.SetControlDown(control);
          canvas->GetEventHandler()->ProcessEvent(click);
        };
        pick(1);
        Require(cfg.GetSelectedFixtures() == std::vector<std::string>{first} &&
            fixtureTable->GetSelectedUuids() == cfg.GetSelectedFixtures(), "Patch click did not select table fixture");
        Menu(ID_Edit_Undo);
        Require(cfg.GetSelectedFixtures().empty(), "Patch selection undo failed");
        Menu(ID_Edit_Redo);
        Require(cfg.GetSelectedFixtures() == std::vector<std::string>{first}, "Patch selection redo failed");
        cfg.MarkSaved();
        pick(181, false, true);
        Require(cfg.GetSelectedFixtures() == (std::vector<std::string>{first, second}), "Ctrl-click did not add fixture");
        pick(1, true);
        Require(cfg.GetSelectedFixtures() == std::vector<std::string>{second}, "Shift-click did not toggle fixture");
        pick(187);
        Require(cfg.GetSelectedFixtures().empty(), "Free channel did not clear selection");
        pick(7);
        Require(cfg.GetSelectedFixtures() == (std::vector<std::string>{first, second}), "Overlap click did not preserve all fixture identities");
        const auto group = scene_grouping::GroupSelection(cfg.GetScene(), {.fixtures = {first, second}});
        Require(group.changed, "Selection group setup failed");
        const auto previousPolicy = selection_movement_settings::LoadInteractiveTransformPolicy(cfg);
        auto groupedPolicy = previousPolicy; groupedPolicy.promoteFixturesToGroup = true;
        selection_movement_settings::SaveInteractiveTransformPolicy(cfg, groupedPolicy);
        cfg.MarkSaved();
        pick(187); hoverPatch(1);
        Require(canvas->Highlights().at(first) == gui::FixtureHoverColor() &&
            canvas->Highlights().at(second) == gui::FixtureRelatedHoverColor(), "Group hover colors did not follow movement policy");
        wxMouseEvent leavePatch(wxEVT_LEAVE_WINDOW); canvas->GetEventHandler()->ProcessEvent(leavePatch);
        pick(1);
        Require(cfg.GetSelectedFixtures() == (std::vector<std::string>{first, second}), "Patch did not select the effective group");
        Require(!cfg.IsDirty(), "Group selection dirtied the project");
        selection_movement_settings::SaveInteractiveTransformPolicy(cfg, previousPolicy);
        scene_grouping::UngroupSelection(cfg.GetScene(), {.groupObjects = {group.groupUuid}});
        cfg.MarkSaved(); pick(187);
        Require(!cfg.IsDirty(), "Patch selection dirtied the project");
        wxCheckBox *compatibility = nullptr;
        for (auto *child : patch_->GetChildren())
          if (auto *checkbox = dynamic_cast<wxCheckBox *>(child)) compatibility = checkbox;
        Require(compatibility && compatibility->GetValue(), "Declared offsets must be enabled by default");
        compatibility->SetValue(true);
        wxCommandEvent preview(wxEVT_CHECKBOX, compatibility->GetId());
        compatibility->GetEventHandler()->ProcessEvent(preview);
        bool provisionalCounter = false;
        for (auto *child : patch_->GetChildren())
          if (auto *text = dynamic_cast<wxStaticText *>(child))
            provisionalCounter |= text->GetLabel().Contains("Provisional:");
        Require(provisionalCounter && !cfg.IsDirty(), "Preview is not explicit/read-only");
        compatibility->SetValue(false); compatibility->GetEventHandler()->ProcessEvent(preview);
        compatibility->SetValue(true); compatibility->GetEventHandler()->ProcessEvent(preview);
        wxComboBox *universes = nullptr;
        for (auto *child : patch_->GetChildren())
          if (auto *combo = dynamic_cast<wxComboBox *>(child)) universes = combo;
        Require(universes, "Universe selector unavailable");
        universes->SetValue("2");
        wxCommandEvent enter(wxEVT_TEXT_ENTER, universes->GetId());
        universes->GetEventHandler()->ProcessEvent(enter); wxYield();
        Require(universes->GetValue() == "2", "Empty universe text disappeared");
        wxCommandEvent select(wxEVT_COMBOBOX, universes->GetId()); select.SetString("1");
        universes->GetEventHandler()->ProcessEvent(select); wxYield();
        cfg.NotifyChange(ConfigManager::Change::Scene); wxYield();
        Require(universes->GetValue() == "1", "Selected universe text disappeared after refresh");
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
        wxUIActionSimulator mouse;
        const auto togglePoint = warnings->ClientToScreen(wxPoint(warnings->GetSize().x / 2, warnings->GetSize().y / 2));
        mouse.MouseMove(togglePoint); mouse.MouseClick(); wxYield();
        for (auto *child : patch_->GetChildren())
          if (auto *candidate = dynamic_cast<wxPopupTransientWindow *>(child))
            Require(!candidate->IsShownOnScreen(), "Second warning-button click reopened diagnostics");
        Menu(ID_View_Layout_Default); break;
      }
      case 2:
        Require(FindPatch(*frame_) == patch_ && patch_->IsShownOnScreen(), "3D switch lost shared panel");
        Require(patch_->GetParent() == manager->GetPane("3DViewport").window, "Patch not under 3D");
        Require(manager->GetPane("3DViewport").IsShown() && !manager->GetPane("2DViewport").IsShown(), "3D pane visibility is wrong");
        FixtureTablePanel::Instance()->PreviewFixtureHover("11111111-1111-4111-8111-111111111111");
        Require(Viewer3DPanel::Instance()->GetHoveredUuid() == "11111111-1111-4111-8111-111111111111", "3D did not receive external hover");
        FixtureTablePanel::Instance()->PreviewFixtureHover({});
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
