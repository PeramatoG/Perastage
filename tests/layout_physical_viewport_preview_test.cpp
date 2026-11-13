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
#include "layout_viewport_preview.h"
#include "layout_viewport_adapter.h"
#include "layout_viewport_properties.h"
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/wx.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <cassert>
#include <cmath>

namespace {
// Exercises the real modal controls, including Cancel and lossless unchanged OK.
void TestViewportPropertiesControls() {
  layouts::Layout2DViewDefinition draft; draft.id=7; draft.frame={17,29,101,73};
  draft.camera.zoom=2.3f;
  Viewer2DViewState visual; visual.viewportWidth=800; visual.viewportHeight=600;
  auto schedule = [](bool accept, int mode, bool changeValues) {
    wxTheApp->CallAfter([=] {
      wxDialog *dialog=nullptr;
      for (auto *window : wxTopLevelWindows)
        if ((dialog=dynamic_cast<wxDialog *>(window))) break;
      assert(dialog);
      std::vector<wxTextCtrl *> fields;
      wxChoice *choice=nullptr; wxComboBox *denominator=nullptr;
      for (auto *child : dialog->GetChildren()) {
        // GTK's wxComboBox inherits wxChoice; keep the two controls distinct.
        if (auto *v=dynamic_cast<wxComboBox *>(child)) { denominator=v; continue; }
        if (auto *v=dynamic_cast<wxChoice *>(child)) choice=v;
        if (auto *v=dynamic_cast<wxTextCtrl *>(child)) fields.push_back(v);
      }
      assert(choice && denominator && fields.size()==6);
      choice->SetSelection(mode);
      wxCommandEvent selected(wxEVT_CHOICE,choice->GetId());
      choice->GetEventHandler()->ProcessEvent(selected);
      if (changeValues) {
        denominator->SetValue("100");
        const std::array<const char *,6> values={"12","-9","7","11","123.456789","88.901234"};
        for(size_t i=0;i<values.size();++i) fields[i]->SetValue(values[i]);
      }
      wxCommandEvent event(wxEVT_BUTTON,accept ? wxID_OK : wxID_CANCEL);
      dialog->GetEventHandler()->ProcessEvent(event);
    });
  };
  schedule(false,1,true);
  assert(!EditLayoutViewportProperties(nullptr,draft,visual));
  assert(draft.scaleMode==layouts::ViewportScaleMode::Free && !draft.physicalScale && draft.frame.width==101);
  schedule(true,1,true);
  assert(EditLayoutViewportProperties(nullptr,draft,visual));
  assert(draft.scaleMode==layouts::ViewportScaleMode::Fixed && draft.physicalScale->denominator==100);
  assert(draft.physicalScale->centerX==12 && draft.physicalScale->centerY==-9);
  assert(draft.camera.zoom==2.3f && draft.id==7);
  const auto f=layouts::ResolveViewportFrame(draft);
  assert(std::abs(f.width*25.4/72-123.456789)<1e-9);
  const auto residual=draft.frameResidual;
  schedule(true,1,false);
  assert(EditLayoutViewportProperties(nullptr,draft,visual));
  assert(draft.frameResidual.width==residual.width && draft.frameResidual.height==residual.height);
  schedule(true,0,false);
  assert(EditLayoutViewportProperties(nullptr,draft,visual));
  assert(draft.scaleMode==layouts::ViewportScaleMode::Free && draft.physicalScale->denominator==100);
  assert(draft.camera.zoom==2.3f);
}
}

// Measures the actual GUI graphics backend under raster scaling/DPI changes.
void TestPhysicalViewportPreview() {
  TestViewportPropertiesControls();
  for (double denominator : {50.0, 100.0}) for (double deviceScale : {1.0, 1.5, 2.0, 4.0}) {
    layouts::Layout2DViewDefinition v; v.frame={0,0,300,200};
    v.scaleMode=layouts::ViewportScaleMode::Fixed;
    v.physicalScale=layouts::PhysicalViewport{denominator,0,0};
    Viewer2DViewState state; viewer2d::AttachPhysicalMapping(v,state);
    viewer2d::Viewer2DRenderMapping mapping;
    const int width=static_cast<int>(300*deviceScale), height=static_cast<int>(200*deviceScale);
    assert(viewer2d::BuildViewMapping(state,width,height,0,mapping));
    CommandBuffer buffer;
    LineCommand line; line.x0=0; line.y0=0; line.x1=1; line.y1=0;
    line.stroke.width=0.02f;
    buffer.commands.emplace_back(line);
    wxBitmap bitmap(width,height,32);
    wxMemoryDC memory; memory.SelectObject(bitmap);
    memory.SetBackground(*wxWHITE_BRUSH); memory.Clear();
    { wxGCDC graphics(memory); RenderPhysicalViewportPreview(graphics,buffer,mapping,nullptr); }
    memory.SelectObject(wxNullBitmap);
    const auto image=bitmap.ConvertToImage(); assert(image.IsOk());
    int left=width,right=-1;
    for(int y=height/2-3; y<=height/2+3; ++y) for(int x=0;x<width;++x)
      if(image.GetRed(x,y)<240) { left=std::min(left,x); right=std::max(right,x); }
    assert(right>=left);
    const double expected=1000/denominator*72/25.4*deviceScale;
    // Pixel coverage includes the authored 0.02 m stroke's end caps.
    const double tolerance = 2 + expected * 0.01;
    assert(std::abs(left-width/2)<tolerance);
    assert(std::abs(right-width/2-expected)<tolerance);
  }
}
