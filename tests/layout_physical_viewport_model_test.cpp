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
#include "LayoutTemplateSerializer.h"
#include <cassert>
#include <cmath>
#include <limits>

void TestPhysicalViewportModel() {
  using namespace layouts;
  auto near = [](double a, double b) { assert(std::abs(a-b) < 1e-10); };
  PhysicalViewport p{50, 2, -3};
  auto m = ResolvePhysicalViewport(p, 300, 200);
  near(m.pointsPerMeter * 25.4 / 72, 20);
  p.denominator = 100;
  near(ResolvePhysicalViewport(p, 300, 200).pointsPerMeter * 25.4 / 72, 10);
  const auto larger = ResolvePhysicalViewport(p, 600, 400);
  near(larger.width, 2*ResolvePhysicalViewport(p,300,200).width);
  near(larger.minX + larger.width/2, 2);
  near(ResolvePhysicalViewport(p, 600,400,2).minX + larger.width/2, -2);
  LayoutDefinition page; page.name = "Physical";
  Layout2DViewDefinition view; view.id=42; view.camera.zoom=2.3f;
  view.camera.viewportWidth=800; view.camera.viewportHeight=600;
  SetViewportFrame(view, {20.123456789, -30.56789, 300.2345, 200.6789});
  view.scaleMode=ViewportScaleMode::Fixed; view.physicalScale=p;
  page.view2dViews.push_back(view);
  auto second=view; second.id=43; second.physicalScale=PhysicalViewport{25,-8,19};
  page.view2dViews.push_back(second);
  auto document=ToTemplateDocument({page});
  std::vector<LayoutDefinition> loaded; std::string error;
  assert(FromTemplateDocument(document,loaded,&error));
  assert(ToTemplateDocument(loaded)==document);
  near(ResolveViewportFrame(loaded[0].view2dViews[0]).width,300.2345);
  loaded[0].view2dViews[0].frame.x+=40;
  assert(loaded[0].view2dViews[0].physicalScale->centerX==2);
  assert(loaded[0].view2dViews[0].physicalScale->denominator==100);
  auto free=view; free.scaleMode=ViewportScaleMode::Free;
  page.view2dViews={free}; auto freeJson=ToJson(page);
  LayoutDefinition reopened; assert(FromJson(freeJson,reopened,&error));
  assert(reopened.view2dViews[0].camera.zoom==2.3f);
  assert(reopened.view2dViews[0].physicalScale->denominator==100);
  auto legacy=free; legacy.physicalScale.reset(); legacy.frameResidual={};
  page.view2dViews={legacy}; auto old=ToJson(page);
  assert(!old["view2dViews"][0].contains("viewport"));
  assert(FromJson(old,reopened,&error));
  assert(reopened.view2dViews[0].scaleMode==ViewportScaleMode::Free);
  assert(ToJson(reopened)==old);
  for(double invalid : {0.0,-1.0, std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::quiet_NaN(),1e20}) {
    auto bad=p; bad.denominator=invalid; bool rejected=false;
    try { ResolvePhysicalViewport(bad,300,200); } catch(const std::invalid_argument &) { rejected=true; }
    assert(rejected);
  }
  for (int field=0;field<4;++field) {
    auto bad=document;
    auto &v=bad["layouts"][0]["view2dViews"][0]["viewport"];
    if(field==0) v["physical"]["denominator"]=0;
    if(field==1) v["physical"]["centerX"]=1e20;
    if(field==2) v["frameResidual"][2]=999;
    if(field==3) v["mode"]="unknown";
    const auto before=ToTemplateDocument(loaded);
    assert(!FromTemplateDocument(bad,loaded,&error) && !error.empty());
    assert(ToTemplateDocument(loaded)==before);
  }
  for(auto f : {ViewportFramePoints{0,0,0,1}, {0,0,1,-1}, {1e20,0,1,1}}) {
    bool rejected=false; try { SetViewportFrame(view,f); } catch(const std::invalid_argument &) { rejected=true; }
    assert(rejected);
  }
  ValidatePhysicalOutput(false,.7,.7);
  ValidatePhysicalOutput(true,1,1);
  bool rejected=false; try { ValidatePhysicalOutput(true,.7,.7); } catch(const std::invalid_argument &) { rejected=true; }
  assert(rejected);
}
