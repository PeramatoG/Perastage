#include "layout_image_export.h"
#include "layouttextutils.h"
#include <wx/app.h>
#include <wx/image.h>
#include <cassert>
#include <fstream>
#include <chrono>
#include <limits>
class InputTestApp : public wxApp { public: bool OnInit() override {return true;} };
wxIMPLEMENT_APP_NO_MAIN(InputTestApp);
void TestPhysicalViewportPreview();
int main(int argc,char **argv) {
  assert(wxEntryStart(argc,argv));assert(wxTheApp->CallOnInit());
  {
    wxInitAllImageHandlers();
    TestPhysicalViewportPreview();
    layouts::LayoutTextDefinition text;text.frame={17,29,101,73};text.text="Plain text";
    auto data=layouttext::BuildLayoutTextExportData(text);
    assert(data.frame.x==17 && data.frame.y==29 && data.frame.width==101 && data.frame.height==73);
    assert(text.text=="Plain text" && text.frame.width==101);
    const auto path=std::filesystem::temp_directory_path()/("layout-image-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".png");
    wxImage source(2,1);source.SetRGB(0,0,255,0,0);source.SetRGB(1,0,0,0,255);source.InitAlpha();source.SetAlpha(0,0,0);source.SetAlpha(1,0,0);
    assert(source.SaveFile(path.string(),wxBITMAP_TYPE_PNG));
    auto read=[&] {std::ifstream f(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
    const auto bytes=read();
    layouts::LayoutImageDefinition image;image.frame={17,29,83,41};image.imagePath=path.string();
    std::string diagnostic;
    for(double scale : {.5,1.,2.}) {
      const auto expected=gui::LayoutImageRasterSize(image.frame,{scale});
      const auto exported=gui::BuildLayoutImageExportData(image,{scale},&diagnostic);
      assert(exported && diagnostic.empty());
      assert(exported->frame.x==17 && exported->frame.y==29 && exported->frame.width==83 && exported->frame.height==41);
      assert(exported->pixelWidth==expected.x && exported->pixelHeight==expected.y);
      assert(exported->pixels[0]==255 && exported->pixels[1]==255 && exported->pixels[2]==255); // Transparent source flattens to white.
      assert(read()==bytes && image.imagePath==path.string());
    }
    auto cap=gui::LayoutImageRasterSize({0,0,1000000,500000},{std::numeric_limits<double>::max()});
    assert(cap.x==8192 && cap.y==4096 && static_cast<long long>(cap.x)*cap.y<=8192LL*8192);
    assert(gui::LayoutImageRasterSize({0,0,0,10},{1}).x<=0);
    image.imagePath.clear();assert(!gui::BuildLayoutImageExportData(image,{1},&diagnostic) && !diagnostic.empty());
    std::filesystem::remove(path);
  }
  wxTheApp->OnExit();wxEntryCleanup();
}
