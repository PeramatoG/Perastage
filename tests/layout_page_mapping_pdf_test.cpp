#include "viewer2dpdfexporter.h"
#include "layouts/PageOutputMapping.h"
#include "PageSetup.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <limits>

namespace {
struct Matrix {
  double a=1,b=0,c=0,d=1,e=0,f=0;
  Matrix Then(Matrix n) const {
    return {a*n.a+c*n.b,b*n.a+d*n.b,a*n.c+c*n.d,b*n.c+d*n.d,
            a*n.e+c*n.f+e,b*n.e+d*n.f+f};
  }
};
struct Box { double x,y,w,h; };
struct Ink { std::vector<Box> rectangles, images; std::vector<double> textSizes; std::vector<std::pair<double,double>> points; };
// Independent PDF CTM interpreter for the generated page operators, not a copy
// of PageOutputMapping. It checks actual final positions after nested q/Q/cm.
Ink Inspect(const std::string &stream) {
  Ink ink; Matrix m; std::vector<Matrix> stack;
  std::istringstream lines(stream); std::string line;
  auto point=[&](double x,double y) { return std::pair{m.a*x+m.c*y+m.e,m.b*x+m.d*y+m.f}; };
  auto box=[&](double x,double y,double w,double h) {
    auto p=point(x,y),q=point(x+w,y+h);
    return Box{std::min(p.first,q.first),std::min(p.second,q.second),std::abs(q.first-p.first),std::abs(q.second-p.second)};
  };
  while(std::getline(lines,line)) {
    if(line=="q") { stack.push_back(m); continue; }
    if(line=="Q") { assert(!stack.empty()); m=stack.back(); stack.pop_back(); continue; }
    std::istringstream tokens(line); std::vector<std::string> t; std::string s;
    while(tokens>>s) t.push_back(s);
    if(t.size()==7 && t.back()=="cm") {
      m=m.Then({std::stod(t[0]),std::stod(t[1]),std::stod(t[2]),std::stod(t[3]),std::stod(t[4]),std::stod(t[5])});
    }
    if(t.size()>=5 && t[4]=="re") ink.rectangles.push_back(box(std::stod(t[0]),std::stod(t[1]),std::stod(t[2]),std::stod(t[3])));
    if(t.size()==2 && t.back()=="Do" && t[0].starts_with("/I")) ink.images.push_back(box(0,0,1,1));
    if(t.size()==3 && t.back()=="Tf") ink.textSizes.push_back(std::stod(t[1])*std::hypot(m.a,m.b));
    if(t.size()==7 && t.back()=="Tm") ink.points.push_back(point(std::stod(t[4]),std::stod(t[5])));
    if(t.size()==3 && t.back()=="Td") ink.points.push_back(point(std::stod(t[0]),std::stod(t[1])));
    if(t.size()==3 && (t.back()=="m" || t.back()=="l")) ink.points.push_back(point(std::stod(t[0]),std::stod(t[1])));
  }
  assert(stack.empty()); return ink;
}
void Near(double a,double b) { assert(std::abs(a-b)<0.003); }
bool Has(const std::vector<Box> &boxes,Box expected) {
  for(auto b:boxes) if(std::abs(b.x-expected.x)<.003 && std::abs(b.y-expected.y)<.003 && std::abs(b.w-expected.w)<.003 && std::abs(b.h-expected.h)<.003) return true;
  return false;
}
std::string PageStream(const std::string &pdf) {
  const auto marker=pdf.find("(Count) Tj"); assert(marker!=std::string::npos);
  const auto begin=pdf.rfind("stream\n",marker)+7;
  return pdf.substr(begin,pdf.find("endstream",begin)-begin);
}
}
int main(int argc,char **argv) {
  LayoutViewExportData view; view.frame={20,30,101,73}; view.viewState.viewportWidth=501; view.viewState.viewportHeight=307;
  LineCommand line; line.x0=-1; line.y0=-1; line.x1=1; line.y1=1;
  view.buffer.commands.emplace_back(line); view.buffer.metadata.push_back({true,false}); view.buffer.sources.push_back("fixture");
  TextCommand label; label.text="ViewLabel"; label.x=0; label.y=0;
  view.buffer.commands.emplace_back(label); view.buffer.metadata.push_back({true,false}); view.buffer.sources.push_back("fixture");
  auto append=[&](CanvasCommand cmd) { view.buffer.commands.push_back(std::move(cmd)); view.buffer.metadata.push_back({true,false}); view.buffer.sources.push_back("fixture"); };
  append(BeginSymbolCommand{"view-symbol"}); append(line); append(EndSymbolCommand{"view-symbol"}); append(PlaceSymbolCommand{"view-symbol",{}});
  LayoutLegendExportData legend; legend.frame={20,30,91,100};
  LayoutLegendItem item; item.typeName=item.displayName="Fixture"; item.count=2; item.symbolKey="legend-test.gdtf"; item.showSideSymbol=true; legend.items={item};
  LayoutEventTableExportData table; table.frame={20,30,111,93}; table.fields[0]="Venue";
  LayoutTextExportData text; text.frame={20,30,151,87}; text.alignment=LayoutTextExportData::Alignment::Center;
  text.lines={{{{"Styled",12,true,true},{" normal",9,false,false}}},{{{"Second",11,false,false}}}};
  LayoutImageExportData image; image.frame={20,30,83,41}; image.pixelWidth=2; image.pixelHeight=1; image.pixels={255,0,0,0,0,255};
  auto outside=text; outside.lines={{{{"Outside",10,false,false}}}}; outside.frame={-11,820,151,87}; outside.zIndex=-1;
  const auto dir=std::filesystem::temp_directory_path()/ ("page-fit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
  std::vector<std::pair<double,double>> referencePoints;
  std::vector<std::string> referenceForms;
  std::ofstream manifest;
  if(argc>1) { manifest.open(std::filesystem::path(argv[1])/"mapping.json"); manifest.precision(17); manifest<<'['; }

  print::PageSetup doc; doc.pageSize=print::PageSize::A4;
  int n=0;
  for(auto source : {print::PageSize::A4,print::PageSize::A3}) for(bool landscape : {false,true}) {
    doc.pageSize=source; doc.landscape=landscape;
    for(auto target : {print::PageSize::A4,print::PageSize::A3}) for(bool orientation : {false,true}) {
      print::PageSetup out; out.pageSize=target; out.landscape=orientation;
      Viewer2DPrintOptions options; options.compressStreams=false; options.floatPrecision=6;
      options.pageWidthPt=out.PageWidthPt(); options.pageHeightPt=out.PageHeightPt();
      options.layoutScaleX=out.PageWidthPt()/doc.PageWidthPt(); options.layoutScaleY=out.PageHeightPt()/doc.PageHeightPt();
      const double scale=std::min(options.layoutScaleX,options.layoutScaleY);
      const auto path=dir/(std::to_string(n++)+".pdf");
      auto result=ExportLayoutToPdf({view},{legend},{table},{text,outside},{image},options,path); assert(result.success);
      std::ifstream f(path,std::ios::binary); std::ostringstream bytes; bytes<<f.rdbuf();
      const auto stream=PageStream(bytes.str()); const auto ink=Inspect(stream);
      std::vector<std::string> forms;
      for(size_t pos=0;(pos=bytes.str().find("/Subtype /Form",pos))!=std::string::npos;) {
        const auto start=bytes.str().find("stream\n",pos)+7, end=bytes.str().find("endstream",start);
        forms.push_back(bytes.str().substr(start,end-start)); pos=end;
      }
      assert(forms.size()==1);
      if(referenceForms.empty()) referenceForms=forms; else assert(referenceForms==forms);
      assert(view.viewState.viewportWidth==501 && view.viewState.viewportHeight==307 && view.frame.width==101);

      for(auto frame : {view.frame,legend.frame,table.frame,text.frame}) {
        const Box expected{frame.x*scale,out.PageHeightPt()-(frame.y+frame.height)*scale,frame.width*scale,frame.height*scale};
        if(!Has(ink.rectangles,expected)) {
          std::cerr<<"Missing mapped frame "<<frame.width<<" at scale "<<scale<<": "<<expected.x<<','<<expected.y<<','<<expected.w<<','<<expected.h<<'\n';
          assert(false);
        }
      }
      assert(Has(ink.rectangles,{-11*scale,out.PageHeightPt()-907*scale,151*scale,87*scale}));
      assert(Has(ink.rectangles,{0,out.PageHeightPt()-doc.PageHeightPt()*scale,doc.PageWidthPt()*scale,doc.PageHeightPt()*scale}));
      assert(stream.find("/KXV0_view_symbol Do")!=std::string::npos || stream.find("view_symbol Do")!=std::string::npos);
      assert(Has(ink.images,{20*scale,out.PageHeightPt()-71*scale,83*scale,41*scale}));
      bool has12=false,has9=false;
      for(double size:ink.textSizes) { has12|=std::abs(size-12*scale)<.003; has9|=std::abs(size-9*scale)<.003; }
      assert(has12 && has9);
      assert(stream.find("0.200000 1")!=std::string::npos); // Existing italic skew.
      assert(stream.find("(ViewLabel) Tj") < stream.find("(Count) Tj"));
      assert(stream.find("(Count) Tj") < stream.find("(Venue:) Tj"));
      assert(stream.find("(Venue:) Tj") < stream.find("(Styled) Tj"));
      assert(stream.find("(Styled) Tj") < stream.find("/I0 Do"));
      assert(result.perastageSymbolInstances==3);
      std::vector<std::pair<double,double>> normalized;
      for(auto [x,y]:ink.points) normalized.push_back({x/scale,(out.PageHeightPt()-y)/scale});
      if(referencePoints.empty()) referencePoints=normalized;
      else { assert(normalized.size()==referencePoints.size()); for(size_t i=0;i<normalized.size();++i) { Near(normalized[i].first,referencePoints[i].first); Near(normalized[i].second,referencePoints[i].second); } }
      std::cout << "doc="<<doc.PageWidthPt()<<'x'<<doc.PageHeightPt()<<" out="<<out.PageWidthPt()<<'x'<<out.PageHeightPt()<<" scale="<<scale;
      for(auto frame : {view.frame,legend.frame,table.frame,text.frame,image.frame})
        std::cout << " [" << frame.x*scale << ',' << out.PageHeightPt()-(frame.y+frame.height)*scale << ',' << frame.width*scale << ',' << frame.height*scale << ']';
      std::cout << '\n';
      auto invalidImage=image; invalidImage.pixels.clear();
  const auto invalidImagePath=dir/"omitted-image.pdf";
  assert(ExportLayoutToPdf({view},{legend},{table},{text},{invalidImage},Viewer2DPrintOptions{},invalidImagePath).success);
  if(argc>1) manifest<<(n>1 ? "," : "")<<"{\"file\":\""<<path.filename().string()<<"\",\"docW\":"<<doc.PageWidthPt()<<",\"docH\":"<<doc.PageHeightPt()<<",\"scale\":"<<scale<<'}';
      if(argc>1) std::filesystem::copy_file(path,std::filesystem::path(argv[1])/path.filename(),std::filesystem::copy_options::overwrite_existing);
    }
  }
  auto invalidImage=image; invalidImage.pixels.clear();
  const auto invalidImagePath=dir/"omitted-image.pdf";
  assert(ExportLayoutToPdf({view},{legend},{table},{text},{invalidImage},Viewer2DPrintOptions{},invalidImagePath).success);
  if(argc>1) manifest<<"]\n";
  for (double bad : {0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
    for (int index=0;index<4;++index) {
      Viewer2DPrintOptions options;
      double *values[]={&options.pageWidthPt,&options.pageHeightPt,&options.layoutScaleX,&options.layoutScaleY};
      *values[index]=bad;
      const auto path=dir/"invalid.pdf";
      {std::ofstream f(path);f<<"original target";}
      const auto result=ExportLayoutToPdf({view},{legend},{table},{text},{image},options,path);
      assert(!result.success && result.message.starts_with("Invalid Layout page mapping:"));
      assert(result.message.find(index<2 ? "output paper dimensions" : "page scale ratios")!=std::string::npos);
      std::ifstream f(path);std::string unchanged;std::getline(f,unchanged);assert(unchanged=="original target");
    }
  }
  std::filesystem::remove_all(dir);
}
