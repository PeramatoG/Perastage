// This file is part of Perastage. License: GPL-3.0-or-later.
#include "LayoutManager.h"
#include "configmanager.h"
#include "mainwindow.h"
#include "mainwindow/ids/mainwindow_command_ids.h"
#include "startup_profile.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <wx/app.h>
#include <wx/apptrait.h>
#include <wx/image.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/timer.h>

namespace {
std::filesystem::path scratch;
constexpr auto kPage = "Properties workspace smoke";
class Paths : public wxStandardPaths {
public:
  wxString GetUserDataDir() const override { return scratch.string(); }
};
class Traits : public wxGUIAppTraits {
  Paths paths_;
public:
  wxStandardPaths &GetStandardPaths() override { return paths_; }
};
void Require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
}
class PropertiesSmoke final : public wxApp {
  MainWindow *frame_ = nullptr;
  wxTimer timer_;
  unsigned ticks_ = 0;
  int result_ = 1;
  int stage_ = 0;

  void Dispatch(int id) {
    wxCommandEvent event(wxEVT_MENU, id);
    frame_->ProcessWindowEvent(event);
  }
  wxTextCtrl &Field(wxWindow &parent, const char *name) {
    auto *control = dynamic_cast<wxTextCtrl *>(wxWindow::FindWindowByName(name, &parent));
    if (!control) throw std::runtime_error(std::string("Missing property field: ") + name);
    return *control;
  }

  void Step(wxTimerEvent &) {
    try {
      Require(++ticks_ < 300, "Properties workspace startup timed out");
      if (frame_->IsStartupInitializationPending()) return;
      auto &config = ConfigManager::Get();
      auto *manager = wxAuiManager::GetManager(frame_);
      Require(manager != nullptr, "Missing AUI manager");
      auto &pane = manager->GetPane("LayoutProperties");
      Require(pane.IsOk(), "Missing LayoutProperties pane");
      Require(pane.window != nullptr, "Missing pane window");
      if (stage_ == 0) {
        Require(pane.IsShown() && pane.dock_direction == wxAUI_DOCK_RIGHT, "Properties must start docked on the right");
        Require(manager->GetPane("LayoutPanel").dock_direction == wxAUI_DOCK_LEFT &&
                manager->GetPane("LayoutViewer").dock_direction == wxAUI_DOCK_CENTER, "Invalid Layout column arrangement");
        const auto revision = config.CaptureDirtyState().revision;
        Dispatch(ID_View_ToggleLayoutProperties);
        Require(!pane.IsShown(), "View menu did not hide Properties");
        for (const int id : {ID_View_Layout_2D, ID_View_Layout_Default, ID_View_Layout_Inspector, ID_View_Layout_Mode}) {
          Dispatch(id);
          Require(!pane.IsShown(), "Workspace switching lost the Properties visibility preference");
        }
        Require(config.CaptureDirtyState().revision == revision, "Pane visibility dirtied the project");
        Dispatch(ID_View_ToggleLayoutProperties);
        Require(pane.IsShown(), "View menu did not restore Properties");
        ++stage_;
        return;
      }
      Require(Field(*pane.window, "Layout name").GetValue() == kPage, "Missing active page context");
      const auto paper = config.GetValue("layouts_collection");
      const auto history = config.CanUndo();
      auto &name = Field(*pane.window, "Layout name");
      wxCommandEvent unchanged(wxEVT_TEXT_ENTER, name.GetId());
      name.ProcessWindowEvent(unchanged);
      Require(config.CanUndo() == history && config.GetValue("layouts_collection") == paper,
              "Unchanged property entry touched Layout history");
      name.ChangeValue("Renamed in Properties");
      name.ProcessWindowEvent(unchanged);
      Require(config.CanUndo() && config.GetValue("layouts_collection") != paper, "Name edit created no Layout mutation");
      Dispatch(ID_Edit_Undo);
      Require(config.GetValue("layouts_collection") == paper, "Undo did not restore Layout name");
      result_ = 0;
      std::cout << "Properties workspace smoke passed" << std::endl;
    } catch (const std::exception &error) {
      std::cerr << error.what() << std::endl;
    }
    timer_.Stop();
    frame_->Destroy();
    ExitMainLoop();
  }
public:
  wxAppTraits *CreateTraits() override { return new Traits; }
  bool OnInit() override {
    scratch = std::filesystem::absolute(argv[1].ToStdString());
    SetAppName("Perastage");
    wxInitAllImageHandlers();
    auto &config = ConfigManager::Get();
    layouts::LayoutDefinition page;
    page.name = kPage;
    Require(layouts::LayoutManager::Get().AddLayout(page), "Cannot create smoke Layout");
    config.SetValue("layout_view_mode", "layout_mode_view");
    config.SetValue("layout_active_name", kPage);
    config.RemoveKey("ui_layout_properties_visible");
    auto project = scratch / "properties.pstg";
    Require(config.SaveProject(project.string()), "Cannot save smoke project");
    frame_ = new MainWindow("Properties workspace smoke", nullptr, std::make_shared<startup::Metrics>());
    SetTopWindow(frame_);
    frame_->LoadStartupProjectFromPath(project.string());
    timer_.SetOwner(this);
    Bind(wxEVT_TIMER, &PropertiesSmoke::Step, this, timer_.GetId());
    timer_.Start(100);
    return true;
  }
  int OnRun() override { wxApp::OnRun(); return result_; }
};
wxIMPLEMENT_APP(PropertiesSmoke);
