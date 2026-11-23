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
#include "LayoutManager.h"
#include "layout_2d_view_capture_service.h"
#include "layout_viewport_preview.h"
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpdfexporter.h"
#include <fstream>
#include "mvrexporter.h"
#include "zip_test_utils.h"

inline void CheckLayoutLabelCapture(MainWindow &window, const std::filesystem::path &scratch) {
  const auto require=[](bool ok,const char *message) { if (!ok) throw std::runtime_error(message); };
  auto &cfg=ConfigManager::Get(); auto &manager=layouts::LayoutManager::Get();
  const auto projectLabels=cfg.GetValue("label_fixture_overrides");
  // Exercise the actual reused editor's scoped draft publication and cancellation.
  bool published=false;
  FixtureLabelEditSession session{_("Current Layout viewport"), {}, {}, {},
      [&](const auto &) { published=true; }};
  {
    FixtureLabelEditor editor(&window,cfg,0,&session);
    auto *offset=dynamic_cast<wxSpinCtrlDouble *>(editor.FindWindowByName("fixtureLabelOffsetX",&editor));
    require(offset,"Scoped editor has no offset control");
    offset->SetValue(1.234);
    wxCommandEvent changed(wxEVT_TEXT,offset->GetId()); changed.SetEventObject(offset);
    offset->GetEventHandler()->ProcessEvent(changed);
    wxCommandEvent apply(wxEVT_BUTTON,wxID_APPLY); editor.GetEventHandler()->ProcessEvent(apply);
    require(!published,"Nested Apply published a local label draft");
    wxCommandEvent cancel(wxEVT_BUTTON,wxID_CANCEL); editor.GetEventHandler()->ProcessEvent(cancel);
    require(!published && cfg.GetValue("label_fixture_overrides")==projectLabels,"Scoped Cancel published project label changes");
  }
  {
    FixtureLabelEditor editor(&window,cfg,0,&session);
    wxCommandEvent ok(wxEVT_BUTTON,wxID_OK); editor.GetEventHandler()->ProcessEvent(ok);
    require(published && cfg.GetValue("label_fixture_overrides")==projectLabels,"Scoped OK did not preserve project label scope");
  }
  auto &fixture = cfg.GetScene().fixtures.begin()->second;
  struct RestoreName {
    ConfigManager &config;
    std::string uuid, original;
    ~RestoreName() {
      auto &fixtures = config.GetScene().fixtures;
      if (auto found = fixtures.find(uuid); found != fixtures.end())
        found->second.instanceName = original;
    }
  } restoreName{cfg, fixture.uuid, fixture.instanceName};
  const std::string bidiName = "Lamp24 مرحبا Rack12 שלום x́ ffi";
  fixture.instanceName = bidiName;
  MvrExporter exporter;
  const auto beforeMvr=scratch/"labels-before.mvr", afterMvr=scratch/"labels-after.mvr";
  require(exporter.ExportCanonicalSnapshotToFile(cfg.GetScene(),beforeMvr.string()),"Baseline canonical MVR export failed");
  std::string beforeXml, afterXml;
  require(tests::zip::ReadEntry(beforeMvr,"GeneralSceneDescription.xml",&beforeXml),"Missing baseline scene XML");
  layouts::FixtureLabelStyleProfile profile{"native-style","Native shared style",{}};
  viewer2d::LabelFieldStyle field; field.size=.22f; field.x=.8f; field.y=.3f;
  field.fontFamily = "Noto Serif";
  field.color={.1f,.2f,.9f}; field.outlineColor={.9f,.7f,.1f}; field.outlineWidth=.02f;
  for (int v=0; v<3; ++v) for (int f=0; f<3; ++f) {
    auto style=field; style.visible=f==1; profile.fields[v][f]=style;
  }
  auto profiles=manager.GetLayouts().labelProfiles; profiles.emplace(profile.id,profile);
  layouts::LayoutDefinition page; page.name="Native scoped labels";
  auto a=viewer2d::CaptureLayoutDefinition(Viewer2DPanel::Instance(),cfg,{10,10,220,250});
  a.id=1; a.labelPresentation.profileId=profile.id;
  a.scaleMode=layouts::ViewportScaleMode::Fixed; a.physicalScale=layouts::PhysicalViewport{100,0,0};
  auto b=a; b.id=2; b.frame.x=240; b.physicalScale->denominator=25;
  for (int f=0; f<3; ++f) { auto s=field; s.size=.12f; s.x=-.6f; s.y-=.6f*f; s.visible=true; s.fontFamily="Source Sans 3"; b.labelPresentation.fields[0][f]=s; }
  page.view2dViews={a,b};
  // Add profiles and assignments through one real document transaction.
  page.view2dViews[0].labelPresentation={}; page.view2dViews[1].labelPresentation={};
  require(manager.AddLayout(page),"Could not create native Layout page");
  require(manager.CommitLabelPresentation(page.name,a,profiles),"Profile assignment failed");
  require(manager.CommitLabelPresentation(page.name,b,profiles),"Local customization failed");
  auto *renderer=window.GetOffscreenRenderer(); require(renderer,"Missing offscreen renderer");
  auto *panel=renderer->GetPanel(); require(panel,"Missing offscreen capture panel");
  const auto capture=[&](const auto &view) {
    LayoutViewExportData data; bool completed=false;
    gui::layoutcapture::Layout2DViewCaptureRequest request;
    request.view=&view; request.capturePanel=panel; request.offscreenRenderer=renderer;
    gui::layoutcapture::Layout2DViewCaptureCallbacks callbacks;
    callbacks.setPanelCaptureInProgress=[](bool){}; callbacks.setCacheCaptureInProgress=[](bool){};
    callbacks.storeRenderState=[](const auto &){};
    callbacks.completeCapture=[&](CommandBuffer buffer, Viewer2DViewState state, auto symbols, int,int,size_t,int) {
      data.buffer=std::move(buffer); data.viewState=state; data.symbolSnapshot=std::move(symbols);
      data.frame=layouts::ResolveViewportFrame(view); completed=true;
    };
    const auto scheduled=gui::layoutcapture::ScheduleLayout2DViewCaptureIfNeeded(request,callbacks);
    require(scheduled.captureScheduled && completed,"Actual Layout capture did not complete");
    return data;
  };
  const auto text=[](const auto &data) {
    std::vector<TextCommand> result;
    for (const auto &c : data.buffer.commands) if (const auto *t=std::get_if<TextCommand>(&c))
      if (t->style.IsComposedGlyph()) result.push_back(*t);
    return result;
  };
  const auto beforeRevision=cfg.GetRenderRevision();
  auto first=capture(a), second=capture(b), again=capture(a);
  const auto ta=text(first), tb=text(second), tc=text(again);
  require(!ta.empty() && tb.size()>ta.size(),"Independent viewport visibility did not reach capture");
  require(ta.size()==tc.size() && ta.front().x==tc.front().x && ta.front().y==tc.front().y,"Cross-viewport capture contamination");
  std::string visual;
  for (const auto &glyph : tb)
    if (!glyph.style.clusterContinuation) visual += glyph.text;
  require(visual.find("Lamp24 ابحرمRack12 םולשx́ ffi") != std::string::npos,
          "Captured Layout lost the mixed-direction fixture name or its clusters");
  require(std::abs(ta.front().style.fontSize-.22)<1e-5 && std::abs(tb.front().style.fontSize-.12)<1e-5,"Captured viewport font sizes disagree");
  require(!ta.front().style.fontResource.empty() &&
          ta.front().style.fontResource != tb.front().style.fontResource &&
          ta.front().style.fontResource == tc.front().style.fontResource,
          "Resolved fonts crossed Layout viewport scopes");
  require(ta.front().style.color.b==field.color[2] && ta.front().style.outlineColor.r==field.outlineColor[0],"Capture lost ink colors");
  require(std::abs(ta.front().style.outlineWidth-.02)<1e-5,"Capture lost outline width");
  require(cfg.GetValue("label_fixture_overrides")==projectLabels,"Capture mutated project overrides");
  require(cfg.GetRenderRevision()==beforeRevision,"Style capture invalidated scene geometry");
  for (const int scale : {1, 2}) {
    const int width = 220 * scale, height = 250 * scale;
    viewer2d::Viewer2DRenderMapping mapping;
    require(viewer2d::BuildViewMapping(second.viewState, width, height, 0, mapping),
            "Could not map the scoped Layout preview");
    wxBitmap bitmap(width, height, 32);
    wxMemoryDC memory(bitmap);
    memory.SetBackground(*wxWHITE_BRUSH);
    memory.Clear();
    { wxGCDC graphics(memory); RenderPhysicalViewportPreview(graphics, second.buffer, mapping,
          second.symbolSnapshot.get(), scale, true); }
    memory.SelectObject(wxNullBitmap);
    const auto image = bitmap.ConvertToImage();
    bool ink = false;
    for (int y = 0; image.IsOk() && y < height && !ink; ++y)
      for (int x = 0; x < width && !ink; ++x)
        ink = image.GetRed(x, y) < 200;
    require(ink, "Captured RTL glyphs were not painted by the Layout preview");
    if (scale == 2) require(image.SaveFile((scratch / "scoped-layout-labels.png").string(), wxBITMAP_TYPE_PNG),
                            "Could not record the Layout preview sample");
  }
  Viewer2DPrintOptions options; options.compressStreams=false;
  const auto pdf=scratch/"scoped-layout-labels.pdf";
  require(ExportLayoutToPdf({first,second},{},{},{},{},options,pdf).success,"Scoped vector PDF export failed");
  std::ifstream pdfFile(pdf,std::ios::binary); const std::string bytes((std::istreambuf_iterator<char>(pdfFile)),{});
  require(bytes.find("%PDF-")==0 && bytes.find("0.100 0.200 0.900 rg")!=std::string::npos,"PDF did not retain captured label color");
  require(bytes.find("/BaseFont /NotoSerif-Regular") != std::string::npos &&
          bytes.find("/BaseFont /SourceSans3-Regular") != std::string::npos,
          "Layout PDF failed to embed the exact captured font faces");
  require(exporter.ExportCanonicalSnapshotToFile(cfg.GetScene(),afterMvr.string()),"Styled canonical MVR export failed");
  require(tests::zip::ReadEntry(afterMvr,"GeneralSceneDescription.xml",&afterXml) && beforeXml==afterXml,"Layout styles mutated canonical MVR scene XML");
  require(cfg.SaveProject((scratch/"label-profiles.pstg").string()),"Scoped PSTG save failed");
  std::string error;
  require(manager.ExportLayoutTemplate(page.name,(scratch/"labels.pslayout").string(),&error),"Portable style template export failed");
  // Collision import must retain both definitions with deterministic identity remapping.
  profiles.at(profile.id).fields[0][1]->size=.31f;
  require(manager.CommitLabelPresentation(page.name,a,profiles),"Shared style edit failed");
  auto changed=capture(a); require(std::abs(text(changed).front().style.fontSize-.31)<1e-5,"Shared edit did not reach capture");
  require(text(capture(b)).size()==tb.size(),"Shared style edit changed explicit local visibility");
  std::string imported;
  require(manager.ImportLayoutTemplate((scratch/"labels.pslayout").string(),"Imported labels",&imported,&error),"Portable style import failed");
  require(manager.GetLayouts().labelProfiles.contains(profile.id+"-2"),"Portable style ID collision was not remapped");
  require(window.LoadProjectFromPath((scratch/"label-profiles.pstg").string(),false),"Scoped PSTG reopen failed");
  require(manager.GetLayouts().labelProfiles.at(profile.id).fields==profile.fields,"PSTG lost profile fields");
  require(cfg.GetScene().fixtures.at(restoreName.uuid).instanceName == bidiName,
          "PSTG changed the logical UTF-8 fixture name");
  require(cfg.GetValue("label_fixture_overrides")==projectLabels,"PSTG replaced project fixture customization");
  std::cout << "Layout label capture passed: two scales, independent fields, shared edits, PDF, PSTG and PSLAYOUT" << std::endl;
}
