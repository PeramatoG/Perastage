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
#include "viewer2dpdfexporter.h"
#include "viewer2dcommandrenderer.h"
#include "layout_viewport_adapter.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
namespace {
class Points final : public viewer2d::IViewer2DCommandBackend {
public:
  std::vector<viewer2d::Viewer2DRenderPoint> points;
  void DrawLine(const viewer2d::Viewer2DRenderPoint &a, const viewer2d::Viewer2DRenderPoint &b,
      const CanvasStroke &, double) override { points.push_back(a); points.push_back(b); }
  void DrawPolyline(const std::vector<viewer2d::Viewer2DRenderPoint> &,const CanvasStroke &,double) override {}
  void DrawPolygon(const std::vector<viewer2d::Viewer2DRenderPoint> &,const CanvasStroke &,const CanvasFill *,double) override {}
  void DrawCircle(const viewer2d::Viewer2DRenderPoint &,double,const CanvasStroke &,const CanvasFill *,double) override {}
  void DrawText(const viewer2d::Viewer2DRenderText &) override {}
};
}
void TestPhysicalViewportPdf() {
  using namespace layouts;
  const auto dir=std::filesystem::temp_directory_path()/ ("physical-pdf-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
  // The float GL camera must cover the exact crop even when its center cannot
  // represent sub-meter offsets at large coordinates.
  for (double center : {-1e7, 1e7}) for (int orientation : {0,1,2}) {
    Layout2DViewDefinition v; v.frame={0,0,1,1}; v.camera.view=orientation;
    v.scaleMode=ViewportScaleMode::Fixed; v.physicalScale=PhysicalViewport{0.01,center,center};
    auto camera=v.camera; viewer2d::PreparePhysicalCapture(v,camera,800,600);
    const auto required=ResolvePhysicalViewport(*v.physicalScale,1,1,orientation);
    const float halfW=800*.5f/(25*camera.zoom), halfH=600*.5f/(25*camera.zoom);
    const float panX=camera.offsetPixelsX/25, panY=camera.offsetPixelsY/25;
    assert(double(-halfW-panX)<=required.minX && double(halfW-panX)>=required.minX+required.width);
    assert(double(-halfH-panY)<=required.minY && double(halfH-panY)>=required.minY+required.height);
  }
  for(double denominator : {25.0,50.0,100.0,200.0,500.0,73.25}) for (int orientation : {0,1,2}) {
    Layout2DViewDefinition v; SetViewportFrame(v, {20.123,30.456,300.234,200.678});
    v.camera.view=orientation; v.camera.zoom=1000;
    v.camera.viewportWidth=10; v.camera.viewportHeight=10;
    v.scaleMode=ViewportScaleMode::Fixed; v.physicalScale=PhysicalViewport{denominator,2,3};
    LayoutViewExportData data; data.frame=ResolveViewportFrame(v); data.drawFrame=false;
    data.viewState.viewportWidth=800; data.viewState.viewportHeight=600;
    viewer2d::AttachPhysicalMapping(v,data.viewState);
    auto switched=data.viewState;
    auto freeView=v; freeView.scaleMode=ViewportScaleMode::Free;
    viewer2d::AttachPhysicalMapping(freeView,switched);
    assert(!switched.physicalMapping && switched.zoom==data.viewState.zoom);
    const double sign=ViewportHorizontalSign(orientation);
    LineCommand line; line.x0=2*sign; line.y0=3; line.x1=3*sign; line.y1=3;
    data.buffer.commands.emplace_back(line); data.buffer.metadata.push_back({true,false}); data.buffer.sources.push_back("physical-line");
    auto vertical=line; vertical.x1=vertical.x0; vertical.y1=vertical.y0+1;
    data.buffer.commands.emplace_back(vertical); data.buffer.metadata.push_back({true,false}); data.buffer.sources.push_back("vertical-line");
    const double expected=1000.0/denominator*72/25.4;
    for(auto dimensions : {std::pair{800,600}, {1600,1200}, {400,900}}) {
      auto camera=v.camera;
      viewer2d::PreparePhysicalCapture(v,camera,dimensions.first,dimensions.second);
      Viewer2DViewState capture;
      capture.viewportWidth=dimensions.first; capture.viewportHeight=dimensions.second;
      capture.zoom=camera.zoom; capture.offsetPixelsX=camera.offsetPixelsX; capture.offsetPixelsY=camera.offsetPixelsY;
      viewer2d::Viewer2DViewBounds bounds;
      assert(viewer2d::ComputeViewBounds(capture,bounds));
      const auto physical=*data.viewState.physicalMapping;
      assert(bounds.width+1e-5>=physical.width && bounds.height+1e-5>=physical.height);
      viewer2d::AttachPhysicalMapping(v,capture);
      viewer2d::Viewer2DRenderMapping mapping;
      assert(viewer2d::BuildViewMapping(capture,data.frame.width,data.frame.height,0,mapping));
      Points preview; viewer2d::Viewer2DCommandRenderer renderer(mapping,preview);
      renderer.Render(data.buffer);
      assert(preview.points.size()==4);
      assert(std::abs(preview.points[3].y-preview.points[2].y+expected)<1e-9);
      assert(std::abs(preview.points[1].x-preview.points[0].x-sign*expected)<1e-9);
      for(double editorZoom : {.25,1.0,3.0}) {
        assert(viewer2d::BuildViewMapping(capture,data.frame.width*editorZoom,data.frame.height*editorZoom,0,mapping));
        assert(std::abs(mapping.scale/editorZoom-expected)<1e-9);
      }
    }
    Viewer2DPrintOptions opts; opts.compressStreams=false; opts.floatPrecision=6;
    const auto path=dir/"view.pdf";
    assert(ExportLayoutToPdf({data},{},{},{},{},opts,path).success);
    std::ifstream input(path,std::ios::binary); std::ostringstream all; all<<input.rdbuf();
    const auto pdf=all.str();
    const auto begin=pdf.rfind("\nstream\n"); assert(begin!=std::string::npos);
    std::istringstream stream(pdf.substr(begin+8,pdf.find("endstream",begin)-begin-8));
    std::vector<std::pair<double,double>> points;
    std::string row;
    bool paperStroke=false;
    while(std::getline(stream,row)) {
      std::istringstream stroke(row); double width; std::string strokeOp;
      if (stroke>>width>>strokeOp && strokeOp=="w" && std::abs(width-0.5625)<1e-6) paperStroke=true;
      std::istringstream tokens(row); double x,y; std::string op;
      if(tokens>>x>>y>>op && (op=="m" || op=="l")) points.emplace_back(x,y);
    }
    assert(paperStroke && "Captured pixel strokes must keep their paper-point width.");
    assert(points.size()>=4);
    assert(std::abs(points[3].second-points[2].second-expected)<0.00001);
    assert(std::abs(points[0].second-(opts.pageHeightPt-data.frame.y-data.frame.height/2))<0.00001);
    assert(std::abs(points[1].first-points[0].first-sign*expected)<0.00001);
    assert(std::abs(points[0].first-(data.frame.x+data.frame.width/2))<0.00001);
    if (denominator==50 && orientation==0) {
      auto other=v;
      other.physicalScale=PhysicalViewport{100,12,-9};
      other.frame.x+=100;
      auto second=data; second.frame=ResolveViewportFrame(other);
      viewer2d::AttachPhysicalMapping(other,second.viewState);
      auto secondLine=line; secondLine.x0=12; secondLine.x1=13;
      secondLine.y0=secondLine.y1=-9;
      second.buffer.commands[0]=secondLine;
      const auto two=dir/"two-views.pdf";
      assert(ExportLayoutToPdf({data,second},{},{},{},{},opts,two).success);
      assert(data.viewState.physicalMapping->pointsPerMeter==2*second.viewState.physicalMapping->pointsPerMeter);
    }
    opts.layoutScaleX=.7; opts.layoutScaleY=.7;
    const auto invalid=dir/"rejected.pdf";
    auto result=ExportLayoutToPdf({data},{},{},{},{},opts,invalid);
    assert(!result.success && result.message.find("100%")!=std::string::npos);
    assert(!std::filesystem::exists(invalid));
  }
  std::filesystem::remove_all(dir);
}
