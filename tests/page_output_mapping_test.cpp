#include "layouts/PageOutputMapping.h"
#include "PageSetup.h"
#include <cassert>
#include <limits>
using namespace layouts::composition;
int main() {
  for (auto source : {print::PageSize::A4,print::PageSize::A3}) for(bool portrait : {false,true}) {
    print::PageSetup doc; doc.pageSize=source; doc.landscape=portrait;
    for(auto target : {print::PageSize::A4,print::PageSize::A3}) for(bool landscape : {false,true}) {
      print::PageSetup out; out.pageSize=target; out.landscape=landscape;
      const auto fit=PageFitForSizes(doc.PageWidthPt(),doc.PageHeightPt(),out.PageWidthPt(),out.PageHeightPt());
      const auto r=fit.Map(Rect{-11,30,101,73});
      assert(r.x==-11*fit.scale && r.y==30*fit.scale && r.width==101*fit.scale && r.height==73*fit.scale);
      const auto corner=fit.Map(Point{doc.PageWidthPt(),doc.PageHeightPt()});
      assert(corner.x<=out.PageWidthPt()+1e-9 && corner.y<=out.PageHeightPt()+1e-9);
      assert(fit.Map(Point{0,0}).x==0 && fit.Map(Point{0,0}).y==0);
    }
  }
  auto fraction=PageFitForSizes(1000,700,333,500);
  assert(std::abs(fraction.Map(Rect{1,2,101,73}).width-33.633)<1e-12);
  assert(std::abs(fraction.Map(Rect{1,2,101,73}).height-24.309)<1e-12);
  auto large=fraction.Map(Rect{1e10,-1e10,1e12,2e12});
  assert(std::isfinite(large.width) && large.width==1e12*.333);
  for(double bad : {0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
    for(int index=0;index<4;++index) {
      double d[]={595,842,842,1191};d[index]=bad;
      bool rejected=false;try{PageFitForSizes(d[0],d[1],d[2],d[3]);}catch(const std::invalid_argument &){rejected=true;}
      assert(rejected);
    }
    bool rejected=false;try{UniformPageFit(bad,1);}catch(const std::invalid_argument &){rejected=true;}assert(rejected);
  }
}
