/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "layout2dviewdialog.h"
#include "layout_fixture_labels.h"
#include "viewer2drenderpanel.h"
#include <exception>

inline void CheckLayoutLabelSelection(MainWindow &window) {
  const auto require=[](bool condition,const char *message) { if (!condition) throw std::runtime_error(message); };
  auto &cfg=ConfigManager::Get();
  const auto projectSelection=cfg.GetSelectedFixtures();
  const auto projectLabels=cfg.GetValue("label_fixture_overrides");
  cfg.ClearHistory();
  const auto revision=cfg.CaptureDirtyState().revision;
  const auto fixture=cfg.GetScene().fixtures.begin()->second;
  auto *previous=Viewer2DPanel::Instance();
  auto *previousRender=Viewer2DRenderPanel::Instance();
  auto view=viewer2d::CaptureLayoutDefinition(previous,cfg,{0,0,220,250});
  Layout2DViewDialog dialog(&window,&cfg,&cfg);
  dialog.SetViewportDefinition(view);
  struct Restore {
    Viewer2DPanel *panel; Viewer2DRenderPanel *render;
    ~Restore() { Viewer2DPanel::SetInstance(panel); Viewer2DRenderPanel::SetInstance(render); }
  } restore{previous,previousRender};
  auto *local=dialog.GetViewerPanel();
  Viewer2DPanel::SetInstance(local); Viewer2DRenderPanel::SetInstance(dialog.GetRenderPanel());
  viewer2d::ScopedViewer2DState state(local,nullptr,cfg,viewer2d::FromLayoutDefinition(view),nullptr,nullptr,false);
  local->SetLayoutEditOverlay(220.f/250,wxSize(220,250));
  dialog.Show();
  wxYield(); local->FitViewToScene(); local->Update();
  CommandBuffer captured;
  local->CaptureFrameNow([&](CommandBuffer buffer,Viewer2DViewState){captured=std::move(buffer);});
  require(!captured.commands.empty(),"Local selection view did not render");
  const auto size=local->GetClientSize(); const wxPoint center(size.x/2,size.y/2);
  const auto pointer=[&](wxEventType type,wxPoint position,bool shift=false) {
    wxMouseEvent event(type); event.SetPosition(position);
    event.m_leftDown=type!=wxEVT_LEFT_UP; event.m_shiftDown=shift;
    local->GetEventHandler()->ProcessEvent(event);
  };
  pointer(wxEVT_LEFT_DOWN,center); pointer(wxEVT_LEFT_UP,center);
  require(local->layoutFixtureLabelSelection==std::vector<std::string>{fixture.uuid},"Clicking fixture did not select its local labels");
  pointer(wxEVT_LEFT_DOWN,center,true); pointer(wxEVT_LEFT_UP,center,true);
  require(local->layoutFixtureLabelSelection->empty(),"Shift-click did not toggle local labels");
  pointer(wxEVT_LEFT_DOWN,center); pointer(wxEVT_LEFT_UP,center);
  const auto camera=local->GetViewState();
  pointer(wxEVT_LEFT_DOWN,center); pointer(wxEVT_MOTION,center+wxPoint(30,20)); pointer(wxEVT_LEFT_UP,center+wxPoint(30,20));
  require(local->layoutFixtureLabelSelection==std::vector<std::string>{fixture.uuid},"Panning changed local fixture targets");
  require(local->GetViewState().offsetPixelsX!=camera.offsetPixelsX,"Local label selection disabled camera panning");
  require(cfg.GetScene().fixtures.at(fixture.uuid).transform.o==fixture.transform.o,"Local selection drag moved scene fixture");
  require(cfg.GetSelectedFixtures()==projectSelection && !cfg.CanUndo(),"Local fixture selection changed project selection/history");

  std::exception_ptr failure;
  wxTheApp->CallAfter([&] {
    for (auto *top : wxTopLevelWindows) if (auto *editor=dynamic_cast<FixtureLabelEditor *>(top)) {
      try {
        auto *scope=dynamic_cast<wxChoice *>(editor->FindWindowByName("fixtureLabelScope",editor));
        require(scope && scope->GetCount()==2 && scope->GetSelection()==1,"Layout editor did not target picked fixtures");
        auto *x=dynamic_cast<wxSpinCtrlDouble *>(editor->FindWindowByName("fixtureLabelOffsetX",editor));
        require(x,"Local label editor has no offset control"); x->SetValue(.73);
        wxCommandEvent changed(wxEVT_TEXT,x->GetId()); changed.SetEventObject(x); x->GetEventHandler()->ProcessEvent(changed);
        wxCommandEvent ok(wxEVT_BUTTON,wxID_OK); editor->GetEventHandler()->ProcessEvent(ok);
      } catch (...) { failure=std::current_exception(); editor->EndModal(wxID_CANCEL); }
      return;
    }
  });
  EditPanelFixtureLabels(dialog.GetRenderPanel(),local,0);
  if (failure) std::rethrow_exception(failure);
  require(local->labelPresentation.fixtures.contains(fixture.uuid),"Picked fixture label edit was not viewport-local");
  require(std::abs(local->labelPresentation.fixtures.at(fixture.uuid)[0][0]->x-.73)<1e-5,"Local fixture edit lost its offset");
  require(cfg.GetValue("label_fixture_overrides")==projectLabels && cfg.GetSelectedFixtures()==projectSelection,
      "Local fixture label edit modified project presentation or selection");
  require(cfg.CaptureDirtyState().revision==revision && !cfg.CanUndo(),"Local draft published dirty state or history");
  dialog.Hide();
  std::cout << "Layout local label selection passed: picking, Shift toggle, pan, editor scope and project isolation" << std::endl;
}
