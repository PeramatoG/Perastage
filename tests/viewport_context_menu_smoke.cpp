/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
// Native popup/command coverage using the production application objects.
#include <wx/wx.h>
#include <wx/apptrait.h>
#include <wx/stdpaths.h>
#include <wx/uiaction.h>
#include <wx/notebook.h>
#include "configmanager.h"
#include "fixturetablepanel.h"
#include "fixtureeditdialog.h"
#include "fixturepatchdialog.h"
#include "mainwindow.h"
#include "mainwindow/ids/mainwindow_command_ids.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include "viewport_context_menu.h"
#include "viewport_scene_editing.h"
#include "startup_profile.h"
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
std::filesystem::path scratch;
const std::string a = "11111111-1111-4111-8111-111111111111";
const std::string b = "22222222-2222-4222-8222-222222222222";
void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
class Paths : public wxStandardPaths {
public: wxString GetUserDataDir() const override { return scratch.string(); }
};
class Traits : public wxGUIAppTraits {
  Paths paths_;
public: wxStandardPaths &GetStandardPaths() override { return paths_; }
};
void Pointer(wxWindow &window, wxEventType type, wxPoint point, bool left = false, bool shift = false) {
  wxMouseEvent event(type); event.SetPosition(point);
  event.m_leftDown = left; event.m_shiftDown = shift;
  event.SetEventObject(&window); window.GetEventHandler()->ProcessEvent(event);
}
void Labels(wxMenu &menu, std::set<wxString> &labels) {
  for (auto *item : menu.GetMenuItems()) {
    labels.insert(item->GetItemLabelText());
    if (item->GetSubMenu()) Labels(*item->GetSubMenu(), labels);
  }
}
bool ContainsText(wxWindow &window, const wxString &value) {
  if (auto *text = dynamic_cast<wxTextCtrl *>(&window); text && text->GetValue() == value) return true;
  for (auto *child : window.GetChildren()) if (ContainsText(*child, value)) return true;
  return false;
}
}
class ContextSmoke : public wxApp {
  MainWindow *frame_ = nullptr;
  wxTimer timer_, dialogTimer_;
  int stage_ = 0, ticks_ = 0, outcome_ = 1, opened_ = 0;
  std::set<wxString> labels_;
  wxString choose_;
  bool editorSeen_ = false, correctEditor_ = false, patchSeen_ = false;
  void Command(int id) { wxCommandEvent event(wxEVT_MENU,id); frame_->ProcessWindowEvent(event); }
  void Observe(wxWindow &window) {
    window.Bind(wxEVT_MENU_OPEN,[this](wxMenuEvent &event) {
      if (event.GetMenu() && !event.GetMenu()->GetParent()) {
        ++opened_; labels_.clear(); Labels(*event.GetMenu(), labels_);
        const auto choice = choose_;
        wxMenu *menu = event.GetMenu();
        CallAfter([menu,choice] {
          // SendEvent exercises wx's temporary popup-selection handler.
          if (!choice.empty()) {
            for (auto *item : menu->GetMenuItems())
              if (item->GetItemLabelText() == choice) { menu->SendEvent(item->GetId()); break; }
          }
          wxUIActionSimulator().Char(WXK_ESCAPE);
        });
      }
      event.Skip();
    });
  }
  void Popup(wxWindow &window, wxPoint point, const char *expected) {
    const int before = opened_;
    Pointer(window,wxEVT_RIGHT_DOWN,point); Pointer(window,wxEVT_RIGHT_UP,point);
    Require(opened_ == before + 1,"Native popup did not open exactly once");
    if (expected && !labels_.contains(wxString::FromUTF8(expected))) {
      std::cerr << "Expected: " << expected << "; actual:";
      for (const auto &label : labels_) std::cerr << " [" << label << "]";
      std::cerr << std::endl;
      throw std::runtime_error("Native menu has wrong target/actions");
    }
    std::cout << "Native popup checked: " << (expected ? expected : "filtered target") << std::endl;
  }
  wxPoint FixturePoint(Viewer2DPanel &panel, const std::string &uuid) {
    const auto size = panel.GetClientSize();
    for (int y = 15; y < size.y - 15; y += 10)
      for (int x = 15; x < size.x - 15; x += 10)
        if (panel.PickFixtureForLayoutLabels({x,y}) == uuid) return {x,y};
    throw std::runtime_error("Fixture cannot be picked in native 2D viewport");
  }
  void CheckPatch() {
    auto &config = ConfigManager::Get();
    config.ClearHistory();
    const auto original = config.GetScene().fixtures.at(a);
    std::string error;
    Require(!gui::viewport::ApplyFixturePatch(config,a,20,"1.513",error) && !error.empty(),"Invalid patch accepted");
    Require(!config.CanUndo() && config.GetScene().fixtures.at(a).address == original.address,"Invalid patch published partially");
    error.clear();
    Require(gui::viewport::ApplyFixturePatch(config,a,20,"2.100",error),"Valid patch rejected");
    Require(config.GetScene().fixtures.at(a).dmxAddresses.has_value(),"Patch did not update canonical assignments");
    Require(!gui::viewport::ApplyFixturePatch(config,a,20,"2.100",error),"No-op patch published");
    config.Undo();
    Require(!config.CanUndo() && config.GetScene().fixtures.at(a).fixtureId == original.fixtureId &&
            config.GetScene().fixtures.at(a).address == original.address,"Patch Undo was not atomic");
    config.Redo(); Require(config.GetScene().fixtures.at(a).address == "2.100","Patch Redo failed");
    config.Undo(); config.ClearHistory();
  }
public:
  wxAppTraits *CreateTraits() override { return new Traits; }
  bool OnInit() override {
    scratch = std::filesystem::absolute(argv[1].ToStdString());
    SetAppName("Perastage"); wxInitAllImageHandlers();
    auto &config = ConfigManager::Get();
    config.SetValue("layout_view_mode","2d_layout_view"); config.SetFloat("view2d_zoom",8);
    Fixture fixture; fixture.uuid = a; fixture.instanceName = "Context Target A";
    fixture.fixtureId = 11; fixture.typeName = "Context Type"; fixture.address = "1.1";
    config.GetScene().fixtures.emplace(a,fixture);
    fixture.uuid = b; fixture.instanceName = "Context Target B"; fixture.fixtureId = 12;
    fixture.transform.o[0] = 2500;
    config.GetScene().fixtures.emplace(b,fixture);
    Require(config.SaveProject((scratch/"context.pstg").string()),"Cannot save synthetic scene");
    frame_ = new MainWindow("Context menu smoke",nullptr,std::make_shared<startup::Metrics>());
    SetTopWindow(frame_); frame_->LoadStartupProjectFromPath((scratch/"context.pstg").string());
    timer_.SetOwner(this,wxID_HIGHEST+1); dialogTimer_.SetOwner(this,wxID_HIGHEST+2);
    Bind(wxEVT_TIMER,&ContextSmoke::Tick,this,timer_.GetId());
    Bind(wxEVT_TIMER,[this](wxTimerEvent &) {
      for (auto *window : wxTopLevelWindows)
        if (auto *dialog = dynamic_cast<FixtureEditDialog *>(window); dialog && dialog->IsModal()) {
          editorSeen_ = true; correctEditor_ = ContainsText(*dialog,"Context Target A");
          dialog->EndModal(wxID_CANCEL);
        } else if (auto *patch = dynamic_cast<FixturePatchDialog *>(window); patch && patch->IsModal()) {
          patchSeen_ = true; patch->EndModal(wxID_CANCEL);
        }
    },dialogTimer_.GetId());
    timer_.Start(100); dialogTimer_.Start(30); return true;
  }
  int OnRun() override { wxApp::OnRun(); return outcome_; }
  void Tick(wxTimerEvent &) {
    try {
      Require(++ticks_ < 700,"Native context smoke timed out");
      auto *panel = Viewer2DPanel::Instance();
      if (frame_->IsStartupInitializationPending() || !panel ||
          (stage_ == 0 && panel->HasPendingResourceLoading())) return;
      auto &config = ConfigManager::Get();
      if (stage_ == 0) {
        timer_.Stop(); Observe(*panel); CheckPatch();
        auto point = FixturePoint(*panel,a);
        frame_->RefreshAfterToolSelectionUpdate({.fixtures = {b,a}});
        const auto ordered = config.GetOrderedSelection();
        config.ClearHistory(); const auto dirty = config.CaptureDirtyState();
        Popup(*panel,point,"Edit Fixture...");
        Require(config.GetSelectedFixtures() == std::vector<std::string>({b,a}),"Context collapsed multi-selection");
        const auto after = config.GetOrderedSelection();
        Require(ordered.size() == after.size(),"Context changed selection chronology length");
        for (size_t i=0;i<ordered.size();++i)
          Require(ordered[i].uuid == after[i].uuid && ordered[i].type == after[i].type,"Context reordered selection");
        Require(!config.CanUndo() && dirty.revision == config.CaptureDirtyState().revision,"Cancelled menu changed document/history");
        choose_ = "Edit Fixture..."; Popup(*panel,point,"Edit Fixture..."); choose_.clear();
        Require(editorSeen_ && correctEditor_,"Context fixture editor targeted wrong UUID");
        Require(!config.CanUndo(),"Cancelled editor added history");
        choose_ = "Edit DMX Patch..."; Popup(*panel,point,"Edit DMX Patch..."); choose_.clear();
        Require(patchSeen_ && !config.CanUndo(),"Patch dialog did not cancel without history");
        const auto groups = config.GetScene().groupObjects.size();
        choose_ = "Group"; Popup(*panel,point,"Group"); choose_.clear();
        Require(config.GetScene().groupObjects.size() == groups + 1 && config.CanUndo(),
                "Context grouping did not dispatch the existing command");
        config.Undo();
        Require(config.GetScene().groupObjects.size() == groups && !config.CanUndo(),
                "Context grouping did not undo in one operation");
        frame_->RefreshAfterToolSceneUpdate(); config.ClearHistory();
        frame_->RefreshAfterToolSelectionUpdate({.fixtures = {b}});
        Popup(*panel,point,"Edit Fixture...");
        Require(config.GetSelectedFixtures() == std::vector<std::string>{a},"Unselected hit did not replace selection");
        Popup(*panel,{5,5},"Select by fixture type");
        const auto hidden = config.GetHiddenLayers();
        config.SetHiddenLayers({config.GetScene().fixtures.at(a).layerUuid});
        panel->UpdateScene(false);
        Popup(*panel,point,nullptr);
        Require(!labels_.contains("Edit Fixture..."),"Hidden fixture became a context target");
        config.SetHiddenLayers(hidden); panel->UpdateScene(false);
        panel->Refresh(false); panel->Update(); point = FixturePoint(*panel,a);
        const int before = opened_;
        Pointer(*panel,wxEVT_RIGHT_DOWN,point); Pointer(*panel,wxEVT_MOTION,point+wxPoint(20,0));
        Pointer(*panel,wxEVT_RIGHT_UP,point+wxPoint(20,0));
        Require(opened_ == before,"Right drag opened a popup");
        Popup(*panel,point,"Edit Fixture...");
        config.SetValue("viewport_cross_table_actions_enabled","1");
        auto *notebook = dynamic_cast<wxNotebook *>(FixtureTablePanel::Instance()->GetParent());
        if (notebook && notebook->GetPageCount() > 1) notebook->SetSelection(1);
        Popup(*panel,point,"Edit Fixture...");
        std::cout << "2D native context checks passed" << std::endl;
        Command(ID_View_Layout_Default); ++stage_; timer_.Start(100); return;
      }
      auto *three = Viewer3DPanel::Instance();
      if (!three || !three->IsShownOnScreen() || three->HasPendingSourceResourceLoading()) return;
      if (stage_ == 1) {
        Observe(*three); three->ResetCameraToIsometric();
        three->GetCamera().SetTarget(0,0,0); three->GetCamera().SetDistance(4);
        three->GetCamera().Update(100);
        three->Refresh(); ++stage_; return;
      }
      timer_.Stop();
      Popup(*three,wxPoint(three->GetClientSize().x/2,three->GetClientSize().y/2),"Edit Fixture...");
      Popup(*three,{5,5},"Render style");
      Require(labels_.contains("Export image..."),"3D export action missing");
      const int before = opened_;
      Pointer(*three,wxEVT_RIGHT_DOWN,{5,5}); Pointer(*three,wxEVT_MOTION,{25,5});
      Pointer(*three,wxEVT_RIGHT_UP,{25,5});
      Require(opened_ == before,"3D right drag opened a popup");
      Popup(*three,{5,5},"Render style");
      // A right click during a camera drag must not open a popup.
      Pointer(*three,wxEVT_LEFT_DOWN,{20,20},true,true);
      const auto cameraBefore = three->GetCamera().GetStateForDiagnostics();
      Pointer(*three,wxEVT_MOTION,{60,40},true,true);
      Require(cameraBefore != three->GetCamera().GetStateForDiagnostics(),"Camera navigation did not move");
      const int navigating = opened_;
      Pointer(*three,wxEVT_RIGHT_DOWN,{60,40},true,true);
      Pointer(*three,wxEVT_RIGHT_UP,{60,40},true,true);
      Require(opened_ == navigating,"Camera navigation opened a popup");
      Pointer(*three,wxEVT_LEFT_UP,{60,40});
      Popup(*three,{5,5},"Render style");
      // Reproduce the stale-navigation path: move the camera, then lose capture.
      Pointer(*three,wxEVT_LEFT_DOWN,{20,20},true,true);
      Pointer(*three,wxEVT_MOTION,{60,40},true,true);
      Require(wxWindow::GetCapture() == three,"Camera navigation did not own capture");
      three->ReleaseMouse();
      wxMouseCaptureLostEvent lost(three->GetId()); three->ProcessWindowEvent(lost);
      Popup(*three,{5,5},"Render style");
      Require(!wxWindow::GetCapture(),"Popup retained mouse capture");
      outcome_ = 0; dialogTimer_.Stop(); frame_->Destroy(); ExitMainLoop();
      std::cout << "Native context menus passed: 2D hit/empty/multi/editor/cancel/scope/drag; 3D popup/drag/capture-loss recovery; patch validation/Undo/Redo" << std::endl;
    } catch (const std::exception &error) {
      std::cerr << "Context menu smoke FAILED: " << error.what() << std::endl;
      timer_.Stop(); dialogTimer_.Stop(); frame_->Destroy(); ExitMainLoop();
    }
  }
};
wxIMPLEMENT_APP(ContextSmoke);
