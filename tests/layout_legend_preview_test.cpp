#include "layoutlegendrenderer.h"
#include "pdf/layout_pdf_legend_renderer.h"
#include "pdf/pdf_label_fonts.h"
#include "legend_composition_adapter.h"
#include "typography/font_resources.h"
#include "symbols/fixture_symbol_availability.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <wx/app.h>

class LegendTestApp : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(LegendTestApp);

using namespace layouts::composition;
static void SamePlan(const LegendCompositionPlan &a, const LegendCompositionPlan &b) {
  assert(a.fontIdentity == b.fontIdentity && a.fontSize == b.fontSize && a.rowHeight == b.rowHeight);
  assert(a.typeColumn.width == b.typeColumn.width && a.typeColumn.x == b.typeColumn.x);
  assert(a.countColumn.x == b.countColumn.x && a.channelColumn.x == b.channelColumn.x);
  assert(a.text.size() == b.text.size() && a.rows.size() == b.rows.size() && a.symbols.size() == b.symbols.size());
  for (size_t i=0; i<a.rows.size(); ++i) {
    assert(a.rows[i].nameLines == b.rows[i].nameLines);
    assert(a.rows[i].frame.y == b.rows[i].frame.y && a.rows[i].frame.height == b.rows[i].frame.height);

  }
  for (size_t i=0; i<a.text.size(); ++i) {
    assert(a.text[i].clip.x == b.text[i].clip.x && a.text[i].clip.y == b.text[i].clip.y && a.text[i].clip.width == b.text[i].clip.width);
    assert(a.text[i].text == b.text[i].text && a.text[i].baseline.x == b.text[i].baseline.x && a.text[i].baseline.y == b.text[i].baseline.y);
  }
  for (size_t i=0; i<a.symbols.size(); ++i) {
    assert(a.symbols[i].view == b.symbols[i].view && a.symbols[i].draw.x == b.symbols[i].draw.x);
    assert(a.symbols[i].draw.y == b.symbols[i].draw.y && a.symbols[i].scale == b.symbols[i].scale);
  }
}
int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv)); assert(wxTheApp->CallOnInit());
  {
    const typography::FontPair bundled(PERASTAGE_TEST_RESOURCE_DIR);
    layout_pdf_internal::LegacyFontPair legacy;
    layout_pdf_internal::PdfFontCatalog catalog{&legacy.regular,&legacy.bold};
    layout_pdf_internal::LabelFonts embedded;
    std::vector<layout_pdf_internal::PdfObject> objects;
    assert(embedded.Prepare(objects,catalog,true));
    assert(embedded.regular.metrics.unitsPerEm == 1000);
    layouts::LayoutLegendDefinition legend;
    const std::vector<std::string> names = {"Exterior Linear-1200 Graze", "MAC Ultra Performance", "Martin Atomic 3000 LED", "ÉclairageSüdÉclairageSüdÉclairageSüd", "É灯😀", "Explicit\nline break"};
    std::vector<layouts::LegendResolvedItem> items;
    for (const auto &name : names) {
      layouts::LegendResolvedItem item;
      item.typeName = item.displayName = name; item.count = 123; item.channelCount = 42;
      item.symbolKey = "legend-test.gdtf"; item.showSideSymbol = true; items.push_back(item);
    }
    for (int n : {0, 1, 3, 5, 20, 100}) for (int width : {120,180,300,600}) {
      auto rows = items;
      rows.resize(n, items[0]);
      auto resources = viewer2d::ResolveLegendComposition(rows, nullptr,
          [](const std::string &key, const std::string &, SymbolViewKind view) {
            // Keep caller-owned handles alive across composition.
            static std::vector<std::shared_ptr<const PerastageSvgSymbolData>> handles;
            handles.push_back(symbol_cache::LoadUsableFixtureSymbol(key, view));
            return handles.back().get();
          });
      resources.input.width=width; resources.input.height=100;
      const auto pdfPlan = ComposeLegend(resources.input, {bundled.regular,bundled.bold});
      if (n == 3 && width == 180) {
        assert((pdfPlan.rows[0].nameLines == std::array<std::string,2>{"Exterior","Linear-1200 Graze"}));
        assert((pdfPlan.rows[1].nameLines == std::array<std::string,2>{"MAC Ultra","Performance"}));
        assert((pdfPlan.rows[2].nameLines == std::array<std::string,2>{"Martin Atomic","3000 LED"}));
      }
      LayoutLegendExportData pdfLegend; pdfLegend.frame={0,0,width,100}; pdfLegend.items=rows;
      for (const double scale : {0.5,1.0,2.0}) {
        std::ostringstream stream;
        LegendCompositionPlan rendered;
        const auto result = layout_pdf_internal::RenderLegendPdf(stream,pdfLegend,nullptr,100*scale,scale,scale,catalog,
            layout_pdf_internal::FloatFormatter{6},
            [](const std::string &key, const std::string &, SymbolViewKind view) {
              static std::vector<std::shared_ptr<const PerastageSvgSymbolData>> handles;
              handles.push_back(symbol_cache::LoadUsableFixtureSymbol(key,view)); return handles.back().get();
            }, {}, &rendered);
        SamePlan(pdfPlan,rendered);
        const layout_pdf_internal::FloatFormatter formatter(6);
        assert(stream.str().find(formatter.Format(scale)+" 0 0 "+formatter.Format(scale)) != std::string::npos);
        for (const auto &line : rendered.text) {
          const auto position = formatter.Format(line.baseline.x)+" "+formatter.Format(100*scale-line.baseline.y)+" Td";
          assert(stream.str().find(position) != std::string::npos);
        }
        assert(result.textSubstitutions == pdfPlan.unsupportedGlyphs);
        for (const auto &line : rendered.text) {
          if (line.glyphRun.unsupportedBytes.empty())
            assert(stream.str().find("("+layout_pdf_internal::EscapePdfLiteralBytes(typography::EncodeWinAnsi(line.text))+") Tj") != std::string::npos);
        }
      }
      for (double zoom : {0.5,0.8,1.0,2.0}) {
        LegendCompositionPlan preview;
        auto image=gui::RenderLegendImage({int(width*zoom),int(100*zoom)}, {width,100},zoom,rows,legend,nullptr,&preview);
        assert(image.IsOk()); SamePlan(preview,pdfPlan);
      }
    }
    if (argc > 1) {
      wxInitAllImageHandlers(); const std::filesystem::path directory(argv[1]);
      items.resize(3);
      for (int width : {120,180,300,600}) {
        LegendCompositionPlan plan;
        auto image=gui::RenderLegendImage({width*4,400},{width,100},4,items,legend,nullptr,&plan);
        assert(image.SaveFile((directory / ("parity-"+std::to_string(width)+"-preview.png")).string(),wxBITMAP_TYPE_PNG));
        assert(image.SaveFile((directory / ("parity-"+std::to_string(width)+"-preview.ppm")).string(),wxBITMAP_TYPE_PNM));
        std::ofstream clips(directory / ("parity-"+std::to_string(width)+"-clips.json"));
        clips << '[';
        for (std::size_t i=0;i<plan.text.size();++i) {
          const auto &r=plan.text[i].clip;
          clips << (i ? "," : "") << '[' << r.x << ',' << r.y << ',' << r.width << ',' << r.height << ']';
        }
        clips << "]\n";
        std::ofstream evidence(directory / ("parity-"+std::to_string(width)+"-plan.txt"));
        for (const auto &row : plan.rows) evidence << row.nameLines[0] << " / " << row.nameLines[1] << '\n';
      }
    }
  }
  wxTheApp->OnExit(); wxEntryCleanup();
}
