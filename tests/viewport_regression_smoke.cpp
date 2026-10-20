// Native smoke against the built application objects; see run_viewport_regression_smoke.py.
#include <wx/wx.h>
#include <wx/apptrait.h>
#include <wx/stdpaths.h>
#include <wx/aui/aui.h>
#include "configmanager.h"
#include "fixture_label_editor.h"
#include "fixturetablepanel.h"
#include "mainwindow.h"
#include "mainwindow/ids/mainwindow_command_ids.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include "startup_profile.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
std::filesystem::path scratch;
constexpr const char *fixtureUuid = "11111111-1111-4111-8111-111111111111";
void Require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
class SmokePaths : public wxStandardPaths {
public:
  wxString GetUserDataDir() const override { return scratch.string(); }
};
class SmokeTraits : public wxGUIAppTraits {
  SmokePaths paths_;
public:
  wxStandardPaths &GetStandardPaths() override { return paths_; }
};
}
class ViewportSmoke : public wxApp {
  MainWindow *frame_ = nullptr;
  FixtureLabelEditor *editor_ = nullptr;
  wxTimer timer_;
  int stage_ = 0, ticks_ = 0, paints_ = 0, resumePaints_ = 0;
  int outcome_ = 1;
  float zoom_ = 0;
  std::string labels_;
  std::vector<unsigned char> selectedPixels_;
  std::shared_ptr<startup::Metrics> metrics_ = std::make_shared<startup::Metrics>();
  void Menu(int id) {
    wxCommandEvent event(wxEVT_MENU, id);
    frame_->GetEventHandler()->ProcessEvent(event);
  }
  std::vector<unsigned char> Pixels() {
    std::vector<unsigned char> pixels; int width, height;
    Require(Viewer2DPanel::Instance()->RenderToRGBA(pixels, width, height, wxSize(600,600)),
            "2D framebuffer capture failed");
    return pixels;
  }
  void Bold() {
    for (wxWindow *child : editor_->GetChildren()) {
      auto *check = dynamic_cast<wxCheckBox *>(child);
      if (check && check->GetLabel() == "Bold") {
        check->SetValue(!check->GetValue());
        wxCommandEvent event(wxEVT_CHECKBOX, check->GetId());
        event.SetEventObject(check); check->GetEventHandler()->ProcessEvent(event);
        return;
      }
    }
    throw std::runtime_error("Editor Bold property missing");
  }
  void Select(const std::vector<std::string> &ids) {
    auto *table = FixtureTablePanel::Instance();
    table->SelectByUuid(ids, false);
    auto *control = table->GetTableCtrl();
    wxDataViewEvent event(wxEVT_DATAVIEW_SELECTION_CHANGED, control, control->GetSelection());
    event.SetEventObject(control); control->GetEventHandler()->ProcessEvent(event);
  }
  void CloseEditor() { editor_->Hide(); editor_->Destroy(); editor_ = nullptr; }
public:
  wxAppTraits *CreateTraits() override { return new SmokeTraits; }
  bool OnInit() override {
    scratch = std::filesystem::absolute(argv[1].ToStdString());
    std::filesystem::create_directories(scratch);
    SetAppName("Perastage"); wxInitAllImageHandlers();
    auto &cfg = ConfigManager::Get();
    if (argc < 3) {
      cfg.SetValue("layout_view_mode", "2d_layout_view");
      cfg.SetFloat("view2d_zoom", 8);
      Fixture fixture; fixture.uuid = fixtureUuid; fixture.instanceName = "Smoke fixture";
      fixture.fixtureId = 1; fixture.address = "1.001"; fixture.typeName = "Smoke";
      cfg.GetScene().fixtures.emplace(fixture.uuid, fixture);
      Require(cfg.SaveProject((scratch/"scene.pstg").string()), "Synthetic PSTG save failed");
    }
    frame_ = new MainWindow("2D viewport regression smoke", nullptr, metrics_);
    SetTopWindow(frame_);
    frame_->LoadStartupProjectFromPath((scratch/"scene.pstg").string());
    timer_.SetOwner(this); Bind(wxEVT_TIMER, &ViewportSmoke::Tick, this); timer_.Start(100);
    return true;
  }
  int OnRun() override { wxApp::OnRun(); return outcome_; }
  void Tick(wxTimerEvent &) {
    try {
      Require(++ticks_ < 600, "GUI smoke timed out");
      auto &cfg = ConfigManager::Get();
      auto *panel = Viewer2DPanel::Instance();
      if (frame_->IsStartupInitializationPending() || !panel || panel->HasPendingResourceLoading()) return;
      auto *manager = wxAuiManager::GetManager(frame_);
      Require(manager && manager->GetPane("2DViewport").IsShown() && panel->IsShownOnScreen(),
              "Actual 2D viewport is not visible");
      Require(panel->IsEnabled() && frame_->IsEnabled(), "Viewport remains disabled after loading");
      std::cout << "Viewport smoke stage " << stage_ << std::endl;
      switch (stage_++) {
      case 0: {
        Require(metrics_->viewer2DConstructions == 1 && metrics_->viewer3DConstructions == 0,
                "Saved 2D startup broke lazy viewport creation");
        panel->Bind(wxEVT_PAINT, [this](wxPaintEvent &e) { ++paints_; e.Skip(); });
        zoom_ = panel->GetViewState().zoom;
        wxMouseEvent wheel(wxEVT_MOUSEWHEEL); wheel.m_wheelRotation = 120; wheel.m_wheelDelta = 120;
        panel->GetEventHandler()->ProcessEvent(wheel);
        Require(panel->GetViewState().zoom > zoom_, "Wheel zoom failed immediately after startup");
        break;
      }
      case 1:
        // Consume a queued paint while project-loading interaction is locked.
        panel->Disable(); panel->SetView(panel->GetView()); panel->Update();
        resumePaints_ = paints_; panel->Enable(); break;
      case 2:
        Require(paints_ > resumePaints_, "Re-enabling did not present the consumed load-time paint");
        Require(frame_->LoadProjectFromPath((scratch/"scene.pstg").string(), true), "Open while in 2D failed");
        break;
      case 3: {
        wxMouseEvent wheel(wxEVT_MOUSEWHEEL); wheel.m_wheelRotation = 120; wheel.m_wheelDelta = 120;
        const float before = panel->GetViewState().zoom;
        panel->GetEventHandler()->ProcessEvent(wheel);
        Require(panel->GetViewState().zoom > before, "Wheel zoom failed after project open");
        Select({fixtureUuid});
        Require(cfg.GetSelectedFixtures() == std::vector<std::string>{fixtureUuid}, "Table selection failed");
        selectedPixels_ = Pixels();
        cfg.SetSelectedFixtures({}); panel->UpdateScene(false);
        Require(Pixels() != selectedPixels_, "Viewport does not display selection outlines");
        Select({fixtureUuid});
        Require(Pixels() == selectedPixels_, "Table re-selection after scene/history synchronization is stale");
        Select({});
        const wxSize size = panel->GetClientSize();
        wxMouseEvent down(wxEVT_LEFT_DOWN); down.SetPosition({size.x/2,size.y/2}); down.m_leftDown = true;
        panel->GetEventHandler()->ProcessEvent(down);
        wxMouseEvent up(wxEVT_LEFT_UP); up.SetPosition(down.GetPosition());
        panel->GetEventHandler()->ProcessEvent(up);
        Require(!cfg.GetSelectedFixtures().empty(), "Picking a fixture from 2D failed");
        break;
      }
      case 4:
        Menu(ID_View_Layout_Default); Menu(ID_View_Layout_2D);
        Menu(ID_View_Layout_Default); Menu(ID_View_Layout_2D);
        Require(Viewer3DPanel::Instance() && !Viewer3DPanel::Instance()->IsShownOnScreen(), "Mode switching failed");
        editor_ = new FixtureLabelEditor(frame_, cfg, 0); editor_->Show(); break;
      case 5: {
        labels_ = cfg.GetValue("label_fixture_overrides").value_or("");
        Bold(); wxCommandEvent apply(wxEVT_BUTTON, wxID_APPLY);
        editor_->GetEventHandler()->ProcessEvent(apply);
        Require(cfg.GetValue("label_fixture_overrides").value_or("") != labels_, "Editor Apply did not commit");
        CloseEditor(); break;
      }
      case 6: {
        CommandBuffer commands;
        panel->CaptureFrameNow([&](CommandBuffer buffer, Viewer2DViewState) { commands = std::move(buffer); });
        bool composed = false;
        for (const auto &command : commands.commands)
          if (const auto *text = std::get_if<TextCommand>(&command))
            composed |= text->style.fontFamily.starts_with("perastage-sans");
        Require(composed, "Edited labels are absent from real viewport capture");
        const auto applied = cfg.GetValue("label_fixture_overrides");
        Menu(ID_Edit_Undo); Require(cfg.GetValue("label_fixture_overrides").value_or("") == labels_, "Undo label change failed");
        Menu(ID_Edit_Redo); Require(cfg.GetValue("label_fixture_overrides") == applied, "Redo label change failed");
        panel->SetSelectedUuids({fixtureUuid}); Pixels();
        labels_ = applied.value_or("");
        editor_ = new FixtureLabelEditor(frame_, cfg, 0); editor_->Show(); break;
      }
      case 7:
        Bold(); CloseEditor();
        Require(cfg.GetValue("label_fixture_overrides").value_or("") == labels_, "Closing committed an unapplied draft");
        cfg.SetValue("layout_perspective", manager->SavePerspective().ToStdString());
        Require(cfg.SaveProject((scratch/"scene.pstg").string()), "Save with a 2D perspective failed");
        Require(frame_->LoadProjectFromPath((scratch/"scene.pstg").string(), false), "Reload with perspective failed");
        break;
      case 8:
        Require(Pixels().size() == 600*600*4, "Viewport capture failed after reload");
        outcome_ = 0; timer_.Stop(); frame_->Destroy(); ExitMainLoop();
        std::cout << "Viewport regression smoke passed" << std::endl; break;
      }
    } catch (const std::exception &error) {
      std::cerr << "Viewport regression smoke FAILED: " << error.what() << std::endl;
      timer_.Stop(); if (editor_) CloseEditor(); frame_->Destroy(); ExitMainLoop();
    }
  }
};
wxIMPLEMENT_APP(ViewportSmoke);
