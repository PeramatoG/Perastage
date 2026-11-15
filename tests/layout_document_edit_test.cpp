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
#include "configmanager.h"
#include "layouts/LayoutDocumentEdit.h"
#include "layouts/LayoutManager.h"
#include "layouts/LayoutTemplateSerializer.h"
#include "layouts/LayoutTemplatePackageService.h"
#include "layouts/LayoutImageResourceRegistry.h"
#include "layout_viewer_selection_state.h"
#include "layout_viewer_group_gesture.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <wx/init.h>

using namespace layouts;
using K = LayoutElementKind;
const std::vector<LayoutElementRef> refs{{K::Text,1},{K::View2D,1},
    {K::Image,1},{K::Legend,1},{K::EventTable,1},{K::View2D,2},{K::View2D,3}};
static double Anchor(ViewportFramePoints f, alignment::Operation op) {
  switch (op) {
  case alignment::Operation::Left: return f.x;
  case alignment::Operation::Right: return f.x+f.width;
  case alignment::Operation::Top: return f.y;
  case alignment::Operation::Bottom: return f.y+f.height;
  case alignment::Operation::CenterHorizontal: return f.x+f.width/2;
  case alignment::Operation::CenterVertical: return f.y+f.height/2;
  }
  std::abort();
}
static void OnlyPositions(const LayoutDefinition &before, const LayoutDefinition &after) {
  auto restored = after;
  for (auto ref : refs) {
    const auto f = ResolveElementFrame(before,ref);
    const auto moved = ResolveElementFrame(after,ref);
    assert(f.width==moved.width && f.height==moved.height);
    SetElementPosition(restored, ref, f.x,f.y);
  }
  assert(ToJson(before)==ToJson(restored));
}
int main() {
  wxInitializer initializer; assert(initializer.IsOk());
  LayoutDefinition page; page.name="Mixed document";
  Layout2DViewDefinition view;
  view.id=1; view.zIndex=9; view.scaleMode=ViewportScaleMode::Fixed;
  view.physicalScale=PhysicalViewport{50,2.5,-3.5};
  view.camera={1,2,3,500,400,1};
  view.layers.hiddenLayers={"hidden-layer"};
  SetViewportFrame(view,{12.25,29.375,111.125,77.25});
  page.view2dViews.push_back(view);
  view.id=2; view.zIndex=5; view.physicalScale=PhysicalViewport{100,-7.25,4.5};
  SetViewportFrame(view,{151.125,81.25,33.25,55.125});
  page.view2dViews.push_back(view);
  view.id=3; view.zIndex=-1; view.scaleMode=ViewportScaleMode::Free;
  view.physicalScale.reset(); view.camera.zoom=7.375;
  SetViewportFrame(view,{51.125,101.25,43.25,25.125});
  page.view2dViews.push_back(view);
  LayoutTextDefinition text; text.id=1; text.zIndex=4; text.frame={50,10,71,39}; text.text="Heading";
  page.textViews.push_back(text);
  LayoutLegendDefinition legend; legend.id=1; legend.zIndex=7; legend.frame={90,51,47,81};
  page.legendViews.push_back(legend);
  LayoutEventTableDefinition table; table.id=1; table.zIndex=2; table.frame={23,85,83,41};
  table.fields[0]="Event"; page.eventTables.push_back(table);
  const auto dir=std::filesystem::temp_directory_path()/
      ("layout-edit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
  { std::ofstream out(dir/"logo.png"); out<<"synthetic image resource"; }
  LayoutImageDefinition image; image.id=1; image.zIndex=3; image.frame={71,39,31,62};
  image.imagePath=(dir/"logo.png").string(); page.imageViews.push_back(image);
  // All reference/operation combinations calculate from one original snapshot.
  for (auto reference : {alignment::Reference::Extents,alignment::Reference::First,alignment::Reference::Last}) {
    for (int index=0;index<6;++index) {
      const auto op=static_cast<alignment::Operation>(index);
      const auto aligned=AlignElements(page,refs,reference,op);
      OnlyPositions(page,aligned);
      std::vector<alignment::Rectangle> rectangles;
      for (auto ref : refs) rectangles.push_back(AlignmentRectangle(ResolveElementFrame(page,ref),false));
      const auto plan=alignment::Calculate(rectangles,reference,op,1e-9);
      const bool horizontal=index>=2 && index<=4;
      for (size_t i=0;i<refs.size();++i) {
        const auto before=ResolveElementFrame(page,refs[i]);
        const auto after=ResolveElementFrame(aligned,refs[i]);
        const double expected=(horizontal?before.x:before.y)+(horizontal?1:-1)*plan.offsets[i];
        const double actual=horizontal?after.x:after.y;
        assert(std::abs(actual-expected)<=0.5+1e-9);
        assert(horizontal?before.y==after.y:before.x==after.x);
      }
      if (reference!=alignment::Reference::Extents) {
        const auto ref=reference==alignment::Reference::First?refs.front():refs.back();
        const auto before=ResolveElementFrame(page,ref), after=ResolveElementFrame(aligned,ref);
        assert(before.x==after.x && before.y==after.y);
        for (auto other : refs)
          assert(std::abs(Anchor(ResolveElementFrame(aligned,other),op)-Anchor(before,op))<=0.5+1e-9);
      }
      const auto repeated=AlignElements(aligned,refs,reference,op);
      if (ToJson(repeated)!=ToJson(aligned)) {
        std::fprintf(stderr,"Repeat changed reference=%d operation=%d\n%s\n",int(reference),index,
            nlohmann::json::diff(ToJson(aligned),ToJson(repeated)).dump().c_str());
      }
      assert(ToJson(repeated)==ToJson(aligned));
      const auto origins=AlignElements(page,refs,reference,op,true);
      OnlyPositions(page,origins);
      assert(ToJson(AlignElements(origins,refs,reference,op,true))==ToJson(origins));
    }
  }
  {
    auto adversarial=page;
    adversarial.textViews[0].frame={-20,-20,25,25};
    adversarial.legendViews[0].frame={37,37,5,5};
    adversarial.imageViews[0].frame={5,5,6,6};
    const std::vector<LayoutElementRef> mixed{{K::Text,1},{K::Legend,1},{K::Image,1}};
    for (int index=0; index<6; ++index) {
      const auto op=static_cast<alignment::Operation>(index);
      const auto aligned=AlignElements(adversarial,mixed,alignment::Reference::Extents,op);
      assert(ToJson(AlignElements(aligned,mixed,alignment::Reference::Extents,op))==ToJson(aligned));
    }
  }
  // Movement preserves all frame dimensions, camera settings and residuals.
  const auto moved=TranslateElements(page,refs,12.75,-8.4);
  OnlyPositions(page,moved);
  for (auto ref : refs) {
    auto a=ResolveElementFrame(page,ref), b=ResolveElementFrame(moved,ref);
    assert(b.x-a.x==13 && b.y-a.y==-8);
  }
  gui::layoutinteraction::LayoutViewerGroupGesture gesture;
  gesture.Begin(page,refs,100,200,2);
  auto preview=gesture.Preview(126,184);
  assert(ToJson(preview)==ToJson(moved));
  gesture.Cancel(); assert(!gesture.Active() && ToJson(page)!=ToJson(preview));
  gesture.Begin(page,refs,100,200,2);
  gesture.Preview(126,184); gesture.Cancel(); // Same cancellation for capture loss.
  assert(!gesture.Active());

  auto &cfg=ConfigManager::Get(); cfg.Reset();
  auto &manager=LayoutManager::Get(); assert(manager.AddLayout(page));
  auto current=[&]() -> LayoutDefinition {
    for (const auto &p : manager.GetLayouts().Items()) if(p.name==page.name) return p;
    std::abort();
  };
  const auto original=current();
  const auto imageResource=original.imageViews[0].projectResourcePath;
  std::vector<std::uint8_t> imageBytes;
  assert(LayoutImageResourceRegistry::Get().GetResourceBytes(imageResource,imageBytes));
  cfg.ClearHistory(); cfg.MarkSaved();
  Fixture fixture; fixture.uuid="scene-fixture"; fixture.transform.o={1.25f,2.5f,-3.75f};
  fixture.transform.u={0,1,0}; fixture.transform.v={-1,0,0};
  fixture.localTransform=fixture.transform; fixture.gdtfSpec="retained.gdtf";
  cfg.GetScene().fixtures[fixture.uuid]=fixture;
  cfg.SetSelectedFixtures({fixture.uuid});
  const auto sceneBefore=cfg.GetScene();
  const auto before=cfg.GetValue("layouts_collection");
  gui::layoutselection::LayoutViewerSelectionState selection;
  for (auto ref : refs) selection.Add(ref);
  selection.Toggle(refs[1]); selection.Toggle(refs[1]);
  assert(!cfg.CanUndo() && !cfg.IsDirty());
  std::string error;
  auto invalid=refs; invalid.push_back({K::Image,999});
  assert(!manager.TranslateElements(page.name,invalid,10,10,&error) && !error.empty());
  assert(!manager.AlignElements(page.name,invalid,alignment::Reference::First,alignment::Operation::Top,false,&error));
  assert(!manager.TranslateElements(page.name,refs,300000,0,&error));
  // Earlier candidates are valid; the late viewport exceeds the page bounds.
  assert(!manager.TranslateElements(page.name,refs,199900,0,&error));
  assert(!manager.TranslateElements(page.name,refs,std::numeric_limits<double>::quiet_NaN(),0,&error));
  assert(!manager.TranslateElements("missing page",refs,10,0,&error));
  assert(cfg.GetValue("layouts_collection")==before && !cfg.CanUndo() && !cfg.IsDirty());
  assert(selection.Ordered().size()==refs.size());
  assert(manager.TranslateElements(page.name,refs,0,0));
  assert(!cfg.CanUndo() && !cfg.IsDirty());
  assert(manager.AlignElements(page.name,refs,alignment::Reference::First,alignment::Operation::Top));
  const auto aligned=current(); OnlyPositions(original,aligned);
  assert(cfg.CanUndo() && cfg.IsDirty());
  assert(manager.AlignElements(page.name,refs,alignment::Reference::First,alignment::Operation::Top));
  cfg.Undo(); assert(ToJson(current())==ToJson(original) && !cfg.CanUndo());
  cfg.Redo(); assert(ToJson(current())==ToJson(aligned) && !cfg.CanRedo());
  cfg.ClearHistory(); cfg.MarkSaved();
  gesture.Begin(current(),refs,20,30,2);
  gesture.Preview(46,14);
  assert(!cfg.CanUndo() && !cfg.IsDirty());
  assert(manager.TranslateElements(page.name,gesture.Refs(),gesture.DeltaX(),gesture.DeltaY()));
  gesture.Cancel();
  const auto dragged=current();
  cfg.Undo(); assert(ToJson(current())==ToJson(aligned) && !cfg.CanUndo());
  cfg.Redo(); assert(ToJson(current())==ToJson(dragged) && !cfg.CanRedo());
  std::vector<std::uint8_t> afterBytes;
  assert(LayoutImageResourceRegistry::Get().GetResourceBytes(imageResource,afterBytes) && afterBytes==imageBytes);
  assert(cfg.GetScene().fixtures.size()==sceneBefore.fixtures.size());
  const auto &unchanged=cfg.GetScene().fixtures.at(fixture.uuid);
  assert(unchanged.transform.o==fixture.transform.o && unchanged.transform.u==fixture.transform.u);
  assert(unchanged.transform.v==fixture.transform.v && unchanged.transform.w==fixture.transform.w);
  assert(unchanged.localTransform.o==fixture.localTransform.o && unchanged.gdtfSpec==fixture.gdtfSpec);
  assert(cfg.GetSelectedFixtures()==std::vector<std::string>{fixture.uuid});
  assert(cfg.GetScene().sceneObjects.size()==sceneBefore.sceneObjects.size());

  assert(manager.ExportLayoutTemplate(page.name,(dir/"mixed.pslayout").string(),&error));
  LayoutTemplateImportResult imported;
  assert(LayoutTemplatePackageService::ImportFile((dir/"mixed.pslayout").string(),imported,&error));
  auto expected=ToJson(dragged), actual=ToJson(imported.layout);
  expected["imageViews"][0]["path"]=actual["imageViews"][0]["path"];
  // Portable packages intentionally omit the author-machine source path.
  expected["imageViews"][0].erase("originalPath");
  assert(expected==actual);
  { std::ofstream out(dir/"legacy.json"); out<<ToTemplateDocument({dragged}); }
  assert(LayoutTemplatePackageService::ImportFile((dir/"legacy.json").string(),imported,&error));
  assert(ToJson(imported.layout)==ToJson(dragged));
  // Real PSTG archive boundary; scene callback is deliberately opaque bytes.
  ProjectSession archive;
  const std::string canonicalScene="opaque canonical MVR: must remain byte-identical";
  const auto project=dir/"mixed.pstg";
  assert(archive.SaveProject(project.string(),[&](auto &bytes) {
    cfg.SaveToFile((dir/"config.json").string());
    std::ifstream in(dir/"config.json",std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(in),{}); return !in.bad();
  },[&](auto &bytes) { bytes.assign(canonicalScene.begin(),canonicalScene.end()); return true; }, [] {
    std::vector<ProjectSession::ArchiveResource> resources;
    for (const auto &r : LayoutImageResourceRegistry::Get().UsedResources()) resources.push_back({r.archivePath,r.bytes});
    return resources;
  }));
  cfg.Reset();
  ProjectSession reopened;
  assert(reopened.LoadProject(project.string(),[&](const std::string &file) { return cfg.LoadFromFile(file); },
      [&](const auto &file) {
        std::ifstream in(file,std::ios::binary);
        const std::string bytes(std::istreambuf_iterator<char>(in),{});
        assert(bytes==canonicalScene); return true;
      }));
  actual=ToJson(current()); expected=ToJson(dragged);
  expected["imageViews"][0]["path"]=actual["imageViews"][0]["path"];
  assert(actual==expected);
  cfg.ClearHistory(); assert(manager.DeleteElements(page.name,refs));
  assert(current().view2dViews.empty() && current().imageViews.empty() && current().textViews.empty());
  cfg.Undo(); assert(ToJson(current())==actual && !cfg.CanUndo());
  assert(LayoutImageResourceRegistry::Get().GetResourceBytes(imageResource,afterBytes) && afterBytes==imageBytes);
  cfg.Reset(); std::filesystem::remove_all(dir);
}
