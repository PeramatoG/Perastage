#include "layout_event_table_renderer.h"
#include "pdf/layout_pdf_event_table_renderer.h"
#include "pdf/pdf_label_fonts.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <wx/app.h>
class TableTestApp : public wxApp { public: bool OnInit() override { return true; } };
wxIMPLEMENT_APP_NO_MAIN(TableTestApp);
using namespace layouts::composition;
void Same(const EventTablePlan &a, const EventTablePlan &b) {
  assert(a.fontIdentity == b.fontIdentity && a.fontSize == b.fontSize);
  assert(a.labelWidth == b.labelWidth && a.valueX == b.valueX && a.rowHeight == b.rowHeight);
  assert(a.clip.width == b.clip.width && a.clip.height == b.clip.height);
  assert(a.diagnostics == b.diagnostics && a.unsupportedGlyphs == b.unsupportedGlyphs);
  for (size_t i = 0; i < 7; ++i) {
    assert(a.rows[i].frame.y == b.rows[i].frame.y);
    for (bool label : {false,true}) {
      const auto &x = label ? a.rows[i].label : a.rows[i].value;
      const auto &y = label ? b.rows[i].label : b.rows[i].value;
      assert(x.source == y.source && x.displayed == y.displayed && x.role == y.role);
      assert(x.baseline.x == y.baseline.x && x.baseline.y == y.baseline.y);
      assert(x.clip.x == y.clip.x && x.clip.y == y.clip.y && x.clip.width == y.clip.width && x.clip.height == y.clip.height);
      assert(x.run.style.size == y.run.style.size && x.run.glyphs.size() == y.run.glyphs.size());
      for (size_t j = 0; j < x.run.glyphs.size(); ++j) {
        assert(x.run.glyphs[j].text == y.run.glyphs[j].text);
        assert(x.run.glyphs[j].x == y.run.glyphs[j].x && x.run.glyphs[j].advance == y.run.glyphs[j].advance);
      }
    }
  }
}
int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv)); assert(wxTheApp->CallOnInit());
  {
    layout_pdf_internal::PdfFontCatalog fonts;
    layout_pdf_internal::LabelFonts embedded;
    std::vector<layout_pdf_internal::PdfObject> objects;
    assert(embedded.Prepare(objects,fonts,true));
    assert(embedded.regular.embedded && embedded.bold.embedded);
    layouts::LayoutEventTableDefinition table;
    table.fields = {"Long venue ÉclairageSüdÉclairageSüd", "Málaga", "É灯😀", "first\nsecond", "", "    ", std::string(500,'W')};
    for (int width : {60,120,300,600}) for (int height : {12,100,300}) {
      table.frame = {10,20,width,height};
      LayoutEventTableExportData data; data.frame = table.frame; data.fields = table.fields;
      EventTablePlan reference;
      for (double scale : {0.5,1.,2.}) {
        std::ostringstream pdf;
        auto p = layout_pdf_internal::RenderEventTablePdf(pdf,data,842*scale,scale,scale,fonts,layout_pdf_internal::FloatFormatter{6});
        if (scale == 0.5) reference = p; else Same(reference,p);
        assert(pdf.str().find(" re W n") != std::string::npos);
        assert(pdf.str().find(" re f") != std::string::npos);
        assert(pdf.str().find(" re S") != std::string::npos);
        assert(pdf.str().find("/F4 ") != std::string::npos);
        auto unlike = layout_pdf_internal::RenderEventTablePdf(pdf,data,842,scale,scale*1.5,fonts,layout_pdf_internal::FloatFormatter{6});
        Same(reference,unlike);
      }
      // A4/A3 and opposite output orientations preserve the same point plan.
      for (const auto sourcePaper : {std::pair{595.,842.}, std::pair{842.,1191.}})
        for (const auto outputPaper : {std::pair{595.,842.}, std::pair{842.,595.}, std::pair{842.,1191.}, std::pair{1191.,842.}}) {
          std::ostringstream pdf;
          Same(reference, layout_pdf_internal::RenderEventTablePdf(pdf,data,outputPaper.second,
              outputPaper.first/sourcePaper.first,outputPaper.second/sourcePaper.second,
              fonts,layout_pdf_internal::FloatFormatter{6}));
        }
      for (double zoom : {0.5,0.8,1.,2.}) {
        EventTablePlan p;
        auto image = gui::RenderEventTableImage({int(width*zoom),int(height*zoom)},zoom,table,&p);
        assert(image.IsOk()); Same(reference,p);
      }
    }
    if (argc > 1) {
      wxInitAllImageHandlers();
      const std::filesystem::path directory(argv[1]);
      table.fields = {"Venue Éclairage", "Málaga", "2026-11-05", "Main stage", "Version 1", "Design", "mail@example.org"};
      for (int width : {120,180,300,600}) {
        table.frame = {0,0,width,100};
        EventTablePlan plan;
        const auto image = gui::RenderEventTableImage({width*4,400},4,table,&plan);
        assert(image.SaveFile((directory / ("parity-"+std::to_string(width)+"-preview.ppm")).string(),wxBITMAP_TYPE_PNM));
        std::ofstream clips(directory / ("parity-"+std::to_string(width)+"-clips.json"));
        clips << '[';
        bool first = true;
        for (const auto &row : plan.rows) for (const auto *cell : {&row.label,&row.value}) {
          const auto &r = cell->clip;
          clips << (first ? "" : ",") << '[' << r.x << ',' << r.y << ',' << r.width << ',' << r.height << ']';
          first = false;
        }
        clips << "]\n";
      }
    }

  }
  wxTheApp->OnExit(); wxEntryCleanup();
}
