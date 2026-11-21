#include "layout_image_export.h"
#include "layouttextutils.h"
#include "typography/font_resources.h"
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
    wxRichTextBuffer rich;
    wxRichTextAttr heading;
    heading.SetAlignment(wxTEXT_ALIGNMENT_RIGHT);
    auto *paragraph = new wxRichTextParagraph(&rich, &heading);
    rich.AppendChild(paragraph);
    wxRichTextAttr first;
    first.SetFontPointSize(18);
    first.SetFontFaceName("Source Sans 3");
    first.SetFontWeight(wxFONTWEIGHT_BOLD);
    first.SetTextColour(wxColour(20, 30, 40));
    wxRichTextAttr second;
    second.SetFontPointSize(10);
    second.SetFontFaceName("Noto Serif");
    second.SetFontUnderlined(true);
    paragraph->AppendChild(new wxRichTextPlainText(wxString::FromUTF8("Producción"), paragraph, &first));
    paragraph->AppendChild(new wxRichTextPlainText(wxString::FromUTF8(" Málaga"), paragraph, &second));
    rich.UpdateRanges();
    text.richText = layouttext::SaveRichTextBufferToString(rich).utf8_string();
    const auto spans = layouttext::BuildLayoutTextExportData(text);
    assert(spans.wrap && spans.lines.size() == 1);
    assert(spans.lines.front().alignment == typography::TextBlock::Alignment::Right);
    assert(spans.lines.front().runs[0].family == "Source Sans 3" && spans.lines.front().runs[0].bold);
    assert(spans.lines.front().runs[0].text == "Producción");
    assert(spans.lines.front().runs[0].color[0] == 20);
    assert(spans.lines.front().runs[1].family == "Noto Serif" && spans.lines.front().runs[1].underline);
    const typography::FontCatalog catalog(PERASTAGE_TEST_RESOURCE_DIR);
    const auto plan = typography::ComposeTextBlock(spans, 200, 140, catalog);
    assert(plan.glyphs.front().glyph.resourceId != plan.glyphs.back().glyph.resourceId);
    assert(plan.glyphs.front().baseline == plan.glyphs.back().baseline);
    assert(!plan.decorations.empty());
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
