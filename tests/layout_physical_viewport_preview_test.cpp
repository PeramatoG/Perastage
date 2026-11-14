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
#include "layout_physical_editor.h"
#include "layout_viewport_properties.h"
#include "layout_view_texture_pixels.h"
#include "layout_view_raster_policy.h"
#include "layout_view_projection.h"
#include <wx/spinbutt.h>
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
  auto schedule = [](bool accept, int mode, bool changeValues, bool resizeOnly = false) {
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
      assert(choice && denominator && fields.size()==4);
      choice->SetSelection(mode);
      wxCommandEvent selected(wxEVT_CHOICE,choice->GetId());
      choice->GetEventHandler()->ProcessEvent(selected);
      for (size_t i=2; i<fields.size(); ++i)
        assert(fields[i]->GetValue().Find('.') == wxNOT_FOUND);
      if (changeValues) {
        denominator->SetValue("100");
        const std::array<const char *,4> values={"12","-9","123.456789","88.901234"};
        for(size_t i=0;i<values.size();++i) fields[i]->SetValue(values[i]);
      }
      if (resizeOnly) fields[2]->SetValue("124");
      wxCommandEvent event(wxEVT_BUTTON,accept ? wxID_OK : wxID_CANCEL);
      dialog->GetEventHandler()->ProcessEvent(event);
    });
  };
  int initialFreePreviews=0;
  schedule(false,1,true);
  assert(!EditLayoutViewportProperties(nullptr,draft,visual,
      [&](const layouts::Layout2DViewDefinition &candidate) { ++initialFreePreviews; draft=candidate; }));
  assert(initialFreePreviews>0);
  assert(draft.scaleMode==layouts::ViewportScaleMode::Free && !draft.physicalScale && draft.frame.width==101);
  schedule(true,1,true);
  assert(EditLayoutViewportProperties(nullptr,draft,visual));
  assert(draft.scaleMode==layouts::ViewportScaleMode::Fixed && draft.physicalScale->denominator==100);
  assert(draft.physicalScale->centerX==12 && draft.physicalScale->centerY==-9);
  assert(draft.camera.zoom==2.3f && draft.id==7);
  const auto f=layouts::ResolveViewportFrame(draft);
  assert(std::abs(f.width*25.4/72-123.456789)<1e-9);
  const auto residual=draft.frameResidual;
  draft.physicalScale->centerX=12.123456789123;
  draft.physicalScale->centerY=-9.987654321987;
  const auto precise=*draft.physicalScale;
  schedule(true,1,false);
  assert(EditLayoutViewportProperties(nullptr,draft,visual));
  assert(draft.frameResidual.width==residual.width && draft.frameResidual.height==residual.height);
  assert(draft.physicalScale->centerX==precise.centerX && draft.physicalScale->centerY==precise.centerY);
  schedule(true,1,false,true);
  assert(EditLayoutViewportProperties(nullptr,draft,visual));
  assert(std::abs(layouts::ResolveViewportFrame(draft).width*25.4/72-124)<1e-9);
  assert(layouts::ResolveViewportFrame(draft).height==f.height);
  schedule(true,0,false);
  assert(EditLayoutViewportProperties(nullptr,draft,visual));
  assert(draft.scaleMode==layouts::ViewportScaleMode::Free && draft.physicalScale->denominator==100);
  assert(draft.camera.zoom==2.3f);
}
}

namespace {
// Preview adapters may temporarily mutate the enclosing editor draft. Every
// candidate must still resolve against the entry snapshot, and Cancel restores it.
void TestLiveViewportProperties() {
  for (int exit : {wxID_OK, wxID_CANCEL, wxID_CLOSE}) {
    layouts::Layout2DViewDefinition draft; draft.id=37; draft.frame={17,29,101,73};
    draft.scaleMode=layouts::ViewportScaleMode::Fixed;
    draft.physicalScale=layouts::PhysicalViewport{50,1.123456789,-2.987654321};
    draft.frameResidual={0.123,-0.234,0.345,-0.456};
    draft.camera.zoom=2.3f;
    const auto original=draft;
    Viewer2DViewState visual; visual.viewportWidth=800; visual.viewportHeight=600;
    int previews=0;
    wxTheApp->CallAfter([&] {
      wxDialog *dialog=nullptr;
      for (auto *window : wxTopLevelWindows)
        if ((dialog=dynamic_cast<wxDialog *>(window))) break;
      assert(dialog);
      wxChoice *mode=nullptr; wxComboBox *denominator=nullptr;
      std::vector<wxTextCtrl *> fields;
      for (auto *child : dialog->GetChildren()) {
        if (auto *v=dynamic_cast<wxComboBox *>(child)) { denominator=v; continue; }
        if (auto *v=dynamic_cast<wxChoice *>(child)) mode=v;
        if (auto *v=dynamic_cast<wxTextCtrl *>(child)) fields.push_back(v);
      }
      assert(mode && denominator && fields.size()==4);
      auto selectMode = [&](int selection) {
        mode->SetSelection(selection);
        wxCommandEvent event(wxEVT_CHOICE,mode->GetId());
        mode->GetEventHandler()->ProcessEvent(event);
      };
      selectMode(0);
      assert(previews>0 && draft.scaleMode==layouts::ViewportScaleMode::Free);
      selectMode(1);
      denominator->SetValue("100");
      assert(draft.physicalScale->denominator==100);
      assert(draft.physicalScale->centerX==0); // Current restored free camera.
      const int validCount=previews;
      denominator->SetValue(""); // Ordinary intermediate typing is not published.
      fields[0]->SetValue("-");
      assert(previews==validCount && draft.physicalScale->denominator==100);
      denominator->SetValue("200");
      assert(previews==validCount);
      fields[0]->SetValue("12.3456789");
      assert(draft.physicalScale->denominator==200 && draft.physicalScale->centerX==12.3456789);
      fields[2]->SetValue("0");
      const int invalidFrameCount=previews;
      fields[2]->SetValue("-1");
      assert(previews==invalidFrameCount);
      fields[2]->SetValue("123.456789");
      assert(previews>invalidFrameCount);
      assert(std::abs(layouts::ResolveViewportFrame(draft).width*25.4/72-123.456789)<1e-9);
      // Editing one rounded field must not round any of the other stored fields.
      assert(draft.frameResidual.x==original.frameResidual.x);
      assert(draft.frameResidual.height==original.frameResidual.height);
      if (exit==wxID_CLOSE) dialog->Close();
      else {
        wxCommandEvent event(wxEVT_BUTTON,exit);
        dialog->GetEventHandler()->ProcessEvent(event);
      }
    });
    const bool accepted=EditLayoutViewportProperties(nullptr,draft,visual,
        [&](const layouts::Layout2DViewDefinition &candidate) { ++previews; draft=candidate; });
    assert(accepted==(exit==wxID_OK) && previews>0);
    assert(draft.id==37 && draft.camera.zoom==2.3f);
    if (accepted) {
      assert(draft.physicalScale->denominator==200 && draft.physicalScale->centerX==12.3456789);
    } else {
      assert(draft.scaleMode==original.scaleMode);
      assert(draft.physicalScale->denominator==50);
      assert(draft.physicalScale->centerX==original.physicalScale->centerX);
      assert(draft.physicalScale->centerY==original.physicalScale->centerY);
      assert(draft.frame.width==101 && draft.frame.height==73);
      assert(draft.frameResidual.width==original.frameResidual.width);
      assert(draft.frameResidual.height==original.frameResidual.height);
    }
  }
}
}

namespace {
void TestViewportNumericArrows() {
  for (int exit : {wxID_OK,wxID_CANCEL}) {
    layouts::Layout2DViewDefinition draft; draft.id=45;
    layouts::SetViewportFrame(draft,{17.123,29.234,301.345,273.456});
    draft.scaleMode=layouts::ViewportScaleMode::Fixed;
    draft.physicalScale=layouts::PhysicalViewport{50,1.123456789,-2.987654321};
    const auto original=draft;
    const auto originalFrame=layouts::ResolveViewportFrame(original);
    Viewer2DViewState visual; visual.viewportWidth=800; visual.viewportHeight=600;
    int previews=0;
    wxTheApp->CallAfter([&] {
      wxDialog *dialog=nullptr;
      for (auto *window : wxTopLevelWindows)
        if ((dialog=dynamic_cast<wxDialog *>(window))) break;
      assert(dialog);
      wxChoice *mode=nullptr; wxComboBox *denominator=nullptr;
      std::vector<wxTextCtrl *> fields; std::vector<wxSpinButton *> arrows;
      for (auto *child : dialog->GetChildren()) {
        if (auto *v=dynamic_cast<wxComboBox *>(child)) { denominator=v; continue; }
        if (auto *v=dynamic_cast<wxChoice *>(child)) mode=v;
        if (auto *v=dynamic_cast<wxTextCtrl *>(child)) fields.push_back(v);
        if (auto *v=dynamic_cast<wxSpinButton *>(child)) arrows.push_back(v);
      }
      assert(mode && denominator && fields.size()==4 && arrows.size()==5);
      auto select = [&](int selection) {
        mode->SetSelection(selection);
        wxCommandEvent event(wxEVT_CHOICE,mode->GetId());
        mode->GetEventHandler()->ProcessEvent(event);
      };
      auto step = [&](size_t index,bool up) {
        wxSpinEvent event(up ? wxEVT_SPIN_UP : wxEVT_SPIN_DOWN,arrows[index]->GetId());
        event.SetEventObject(arrows[index]);
        arrows[index]->GetEventHandler()->ProcessEvent(event);
      };
      select(0);
      for(size_t i=0;i<5;++i) assert(arrows[i]->IsEnabled()==(i>=3));
      select(1);
      assert(draft.physicalScale->centerX==0);
      fields[0]->SetValue("1.123456789");
      step(0,true); assert(draft.physicalScale->denominator==51);
      step(0,false); assert(draft.physicalScale->denominator==50);
      step(1,true);
      assert(std::abs(draft.physicalScale->centerX-original.physicalScale->centerX-0.01)<1e-12);
      step(1,false); assert(draft.physicalScale->centerX==original.physicalScale->centerX);
      step(3,true);
      assert(std::abs((layouts::ResolveViewportFrame(draft).width-originalFrame.width)*25.4/72-1)<1e-12);
      assert(fields[2]->GetValue().Find('.')==wxNOT_FOUND);
      step(3,false); assert(draft.frameResidual.width==original.frameResidual.width);
      step(4,false);
      assert(std::abs((layouts::ResolveViewportFrame(draft).height-originalFrame.height)*25.4/72+1)<1e-12);
      step(4,true); assert(draft.frameResidual.height==original.frameResidual.height);
      assert(draft.frame.x==original.frame.x && draft.frame.y==original.frame.y);
      assert(draft.frameResidual.x==original.frameResidual.x && draft.frameResidual.y==original.frameResidual.y);
      fields[2]->SetValue("123.456789"); step(3,true);
      assert(std::abs(layouts::ResolveViewportFrame(draft).width*25.4/72-124.456789)<1e-9);
      step(3,false);
      assert(std::abs(layouts::ResolveViewportFrame(draft).width*25.4/72-123.456789)<1e-9);
      fields[2]->SetValue(""); fields[2]->SetValue("123");
      assert(std::abs(layouts::ResolveViewportFrame(draft).width*25.4/72-123)<1e-12);
      fields[0]->SetValue("1.234567891234"); step(1,true);
      assert(std::abs(draft.physicalScale->centerX-1.244567891234)<1e-14);
      step(1,false); assert(draft.physicalScale->centerX==1.234567891234);
      fields[0]->SetValue("-"); const int validCount=previews;
      step(1,true); assert(previews==validCount && fields[0]->GetValue()=="-");
      fields[0]->SetValue("0.5");
      denominator->SetValue("0.01"); step(0,false);
      assert(draft.physicalScale->denominator==0.01);
      step(0,true); assert(draft.physicalScale->denominator==1.01);
      fields[2]->SetValue("1"); step(3,false);
      assert(std::abs(layouts::ResolveViewportFrame(draft).width-1)<1e-12);
      wxCommandEvent event(wxEVT_BUTTON,exit);
      dialog->GetEventHandler()->ProcessEvent(event);
    });
    const bool accepted=EditLayoutViewportProperties(nullptr,draft,visual,
        [&](const layouts::Layout2DViewDefinition &candidate) { ++previews; draft=candidate; });
    assert(accepted==(exit==wxID_OK) && previews>0);
    if (accepted) assert(draft.physicalScale->centerX==0.5 && draft.physicalScale->denominator==1.01);
    else {
      assert(draft.physicalScale->centerX==original.physicalScale->centerX);
      assert(draft.physicalScale->denominator==50);
      assert(draft.frameResidual.width==original.frameResidual.width);
    }
  }
}
}

namespace {
void TestScaleModeKeepsFreeCenter() {
  for(int orientation : {0,1,2}) for(bool accept : {false,true}) {
    layouts::Layout2DViewDefinition draft; draft.frame={17,29,300,200};
    draft.scaleMode=layouts::ViewportScaleMode::Free;
    draft.physicalScale=layouts::PhysicalViewport{200,99,-88}; // Remembered fixed framing.
    Viewer2DViewState visual; visual.viewportWidth=800; visual.viewportHeight=600;
    visual.view=static_cast<Viewer2DView>(orientation);
    visual.offsetPixelsX=-250; visual.offsetPixelsY=75;
    const double centerX=layouts::ViewportHorizontalSign(orientation)*10, centerY=-3;
    wxTheApp->CallAfter([&] {
      for(auto *window:wxTopLevelWindows) if(auto *dialog=dynamic_cast<wxDialog *>(window)) {
        wxCommandEvent event(wxEVT_BUTTON,wxID_OK);
        dialog->GetEventHandler()->ProcessEvent(event); break;
      }
    });
    assert(EditLayoutViewportProperties(nullptr,draft,visual));
    assert(draft.scaleMode==layouts::ViewportScaleMode::Free);
    assert(draft.physicalScale->centerX==99 && draft.physicalScale->centerY==-88);
    wxTheApp->CallAfter([&] {
      wxDialog *dialog=nullptr;
      for(auto *window:wxTopLevelWindows)
        if((dialog=dynamic_cast<wxDialog *>(window))) break;
      assert(dialog);
      wxChoice *mode=nullptr; std::vector<wxTextCtrl *> fields;
      for(auto *child:dialog->GetChildren()) {
        if(dynamic_cast<wxComboBox *>(child)) continue;
        if(auto *v=dynamic_cast<wxChoice *>(child)) mode=v;
        if(auto *v=dynamic_cast<wxTextCtrl *>(child)) fields.push_back(v);
      }
      assert(mode && fields.size()==4);
      auto select=[&](int selection) {
        mode->SetSelection(selection);
        wxCommandEvent event(wxEVT_CHOICE,mode->GetId());
        mode->GetEventHandler()->ProcessEvent(event);
      };
      select(1);
      assert(draft.physicalScale->centerX==centerX && draft.physicalScale->centerY==centerY);
      assert(draft.physicalScale->denominator==200);
      fields[0]->SetValue("17.25"); fields[1]->SetValue("-8.5");
      select(0); select(1);
      assert(draft.physicalScale->centerX==centerX && draft.physicalScale->centerY==centerY);
      wxCommandEvent end(wxEVT_BUTTON,accept ? wxID_OK : wxID_CANCEL);
      dialog->GetEventHandler()->ProcessEvent(end);
    });
    assert(EditLayoutViewportProperties(nullptr,draft,visual,
        [&](const layouts::Layout2DViewDefinition &candidate){ draft=candidate; })==accept);
    if(accept) assert(draft.physicalScale->centerX==centerX && draft.physicalScale->centerY==centerY);
    else assert(draft.physicalScale->centerX==99 && draft.scaleMode==layouts::ViewportScaleMode::Free);
  }
}
void TestNativePhysicalPreviewContract() {
  using namespace gui::layoutraster;
  layouts::Layout2DViewDefinition view; view.frame={0,0,300,200};
  view.scaleMode=layouts::ViewportScaleMode::Fixed;
  view.physicalScale=layouts::PhysicalViewport{50,12.123456789,-3.987654321};
  Layout2DViewRasterRequest request; request.view=&view; request.renderSize={300,200}; request.contentHash=5;
  Layout2DViewRasterCacheInput cache; cache.restoredFromPersistentCache=true;
  std::vector<unsigned char> saved(300*200*4,255);
  cache.persistentRgba=&saved; cache.persistentRgbaSize={300,200};
  cache.persistentRgbaRenderZoom=1; cache.persistentRgbaContentHash=5;
  assert(!NeedsLiveViewportScene(request,cache) && !UseCommandBufferRaster(request,cache));
  request.renderSize={600,400};
  assert(NeedsLiveViewportScene(request,cache) && !UseCommandBufferRaster(request,cache));
  view.scaleMode=layouts::ViewportScaleMode::Free;
  assert(!NeedsLiveViewportScene(request,cache) && UseCommandBufferRaster(request,cache));
  cache.restoredFromPersistentCache=false;
  assert(NeedsLiveViewportScene(request,cache) && !UseCommandBufferRaster(request,cache));
  for(int orientation:{0,1,2}) for(double denominator:{50.0,100.0})
    for(auto size:{wxSize(300,200),wxSize(600,400),wxSize(801,599)}) {
      const auto physical=layouts::ResolvePhysicalViewport({denominator,12.123456789,-3.987654321},300,200,orientation);
      const viewer2d::CoordinateTransform camera{size.x,size.y,999,1234,-5678,static_cast<Viewer2DView>(orientation)};
      const auto projection=viewer2d::ResolveLayoutProjectionBounds(camera,physical);
      assert(projection);
      Viewer2DViewState state; state.physicalMapping=physical;
      viewer2d::Viewer2DRenderMapping mapping;
      assert(viewer2d::BuildViewMapping(state,size.x,size.y,0,mapping));
      const double x=physical.minX+physical.width/2, y=physical.minY+physical.height/2;
      const double px=(x-projection->left)/(projection->right-projection->left)*size.x;
      const double py=size.y-(y-projection->bottom)/(projection->top-projection->bottom)*size.y;
      assert(std::abs(px-size.x/2.0)<1e-9 && std::abs(py-size.y/2.0)<1e-9);
      assert(std::abs(size.x/(projection->right-projection->left)-mapping.scale)<1e-9);
      assert(std::abs(size.y/(projection->top-projection->bottom)-mapping.scale)<1e-9);
      const auto legacy=viewer2d::ComputeOrthographicBounds(camera);
      const auto resolved=viewer2d::ResolveLayoutProjectionBounds(camera,std::nullopt);
      assert(legacy && resolved && legacy->left==resolved->left && legacy->top==resolved->top);
    }
}
}

// Measures the actual GUI graphics backend under raster scaling/DPI changes.
void TestPhysicalViewportPreview() {
  TestViewportPropertiesControls();
  TestLiveViewportProperties();
  TestViewportNumericArrows();
  TestScaleModeKeepsFreeCenter();
  TestNativePhysicalPreviewContract();
  const layouts::PhysicalViewport physical{50,12.123456789,-3.987654321};
  const layouts::ViewportFramePoints frame{0,0,312.3456,197.6543};
  for (auto size : {wxSize(800,600),wxSize(1600,1200),wxSize(600,900)}) {
    const auto geometry=viewer2d::ResolvePhysicalEditorGeometry(
        physical,frame,size.x,size.y,viewer2d::kViewer2DPixelsPerMeter);
    const auto world=layouts::ResolvePhysicalViewport(physical,frame.width,frame.height);
    assert(std::abs(geometry.width/geometry.zoom/viewer2d::kViewer2DPixelsPerMeter-world.width)<1e-10);
    assert(std::abs(geometry.height/geometry.zoom/viewer2d::kViewer2DPixelsPerMeter-world.height)<1e-10);
  }
  for (int orientation : {0,1,2}) {
    const auto unchanged=viewer2d::ResolvePhysicalEditorPan(physical,0,0,orientation,viewer2d::kViewer2DPixelsPerMeter);
    assert(unchanged.centerX==physical.centerX && unchanged.centerY==physical.centerY);
    const auto pan=viewer2d::ResolvePhysicalEditorPan(physical,25,-50,orientation,viewer2d::kViewer2DPixelsPerMeter);
    assert(pan.denominator==50);
    assert(std::abs(pan.centerX-physical.centerX+layouts::ViewportHorizontalSign(orientation))<1e-12);
    assert(pan.centerY==physical.centerY+2);
  }
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
    line.stroke.width=1; // Production grid/ruler/mesh strokes are captured pixels.
    buffer.commands.emplace_back(line);
    buffer.metadata.push_back({true,false});
    PolygonCommand triangle; triangle.points={-1,-1,-0.5f,-1,-1,-0.5f};
    triangle.hasFill=true; triangle.fill.color={0,1,0,1}; triangle.stroke.width=1000;
    buffer.commands.emplace_back(triangle); buffer.metadata.push_back({false,true});
    TextCommand glyph; glyph.x=-1; glyph.y=0.5f; glyph.text="F";
    glyph.style.fontFamily="perastage-sans-bold"; glyph.style.fontSize=0.3f;
    buffer.commands.emplace_back(glyph); buffer.metadata.push_back({false,true});
    wxBitmap bitmap(width,height,32);
    wxMemoryDC memory; memory.SelectObject(bitmap);
    memory.SetBackground(*wxWHITE_BRUSH); memory.Clear();
    { wxGCDC graphics(memory); RenderPhysicalViewportPreview(graphics,buffer,mapping,nullptr,deviceScale); }
    memory.SelectObject(wxNullBitmap);
    const auto image=bitmap.ConvertToImage(); assert(image.IsOk());
    // Production wx-image -> GL-texture conversion with asymmetric geometry
    // and an upright glyph. DrawView samples its top at v=1.
    std::vector<unsigned char> texture;
    assert(gui::layoutcache::CopyLayoutViewTexturePixels(image,texture));
    for (int y=0; y<height; ++y) for (int x=0; x<width; ++x) {
      const size_t sampled=(static_cast<size_t>(height-1-y)*width+x)*4;
      assert(texture[sampled]==image.GetRed(x,y));
      assert(texture[sampled+1]==image.GetGreen(x,y));
      assert(texture[sampled+2]==image.GetBlue(x,y));
    }
    const int outsideX=static_cast<int>(width/2-1.1*mapping.scale);
    const int outsideY=static_cast<int>(height/2+0.75*mapping.scale);
    assert(image.GetRed(outsideX,outsideY)==255 && image.GetGreen(outsideX,outsideY)==255);
    int left=width,right=-1, top=height,bottom=-1;
    for(int y=height/2-3; y<=height/2+3; ++y) for(int x=0;x<width;++x)
      if(image.GetRed(x,y)<240) { left=std::min(left,x); right=std::max(right,x); }
    assert(right>=left);
    const double expected=1000/denominator*72/25.4*deviceScale;
    for(int y=0;y<height;++y)
      if(image.GetRed(width/2+static_cast<int>(expected/2),y)<240) {
        top=std::min(top,y); bottom=std::max(bottom,y);
      }
    // Ink thickness stays in paper points as the drawing denominator changes.
    assert(bottom-top+1 <= std::ceil(0.5625*deviceScale)+2);
    // Raster antialias coverage is bounded independently of world scale.
    const double tolerance = 2 + expected * 0.01;
    assert(std::abs(left-width/2)<tolerance);
    assert(std::abs(right-width/2-expected)<tolerance);
  }
}
