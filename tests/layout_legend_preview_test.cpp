#include "layoutlegendrenderer.h"
#include "pdf/layout_pdf_legend_renderer.h"
#include "pdf/pdf_label_fonts.h"
#include "legend_composition_adapter.h"

#include <cassert>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <wx/app.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>

class LegendTestApp : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(LegendTestApp);

int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp->CallOnInit());
  {
    using namespace layouts::composition;
    layouts::LayoutLegendDefinition legend;
    std::vector<layouts::LegendResolvedItem> items(1);
    items[0].typeName = items[0].displayName = "Profile fixture";
    items[0].count = 123; items[0].channelCount = 42;
    items[0].symbolKey = "legend-test.gdtf";
    auto normal = gui::RenderLegendImage({300,100}, {300,100}, 1, items, legend, nullptr);
    items[0].showSideSymbol = true;
    auto side = gui::RenderLegendImage({300,100}, {300,100}, 1, items, legend, nullptr);
    assert(normal.IsOk() && side.IsOk());
    assert(std::memcmp(normal.GetData(), side.GetData(), 300 * 100 * 3) != 0);
    if (argc > 1) {
      wxInitAllImageHandlers();
      const std::filesystem::path directory(argv[1]);
      assert(normal.SaveFile((directory / "legend-bottom-front.png").string(), wxBITMAP_TYPE_PNG));
      assert(side.SaveFile((directory / "legend-three-views.png").string(), wxBITMAP_TYPE_PNG));
    }
    items[0].displayName = "Éclairage 灯 long label with many words";
    for (double zoom : {0.5, 1.0, 2.0}) {
      assert(gui::RenderLegendImage({int(120*zoom),int(100*zoom)}, {120,100}, zoom,
          items, legend, nullptr).IsOk());
    }
    for (int n : {0,1,20,100}) {
      items.resize(n);
      for (auto &item : items) {
        item.count = 123; item.displayName = "Profile fixture long label"; item.channelCount = 42;
      }
      wxBitmap bitmap(300,100,32);
      wxMemoryDC memory(bitmap); wxGCDC dc(memory);
      auto resources = viewer2d::ResolveLegendComposition(items, nullptr, {});
      resources.input.width = 300; resources.input.height = 100;
      const auto preview = ComposeLegend(resources.input, [&](std::string_view text, double size, TextRole role) {
        return gui::MeasureLegendPreviewText(dc, text, size, role);
      });
      std::cout << n << " rows: font=" << preview.fontSize << " row=" << preview.rowHeight
                << " Count=" << preview.countColumn.x << " Type=" << preview.typeColumn.x
                << " Ch=" << preview.channelColumn.x << " typeWidth=" << preview.typeColumn.width << '\n';
      layout_pdf_internal::LegacyFontPair fonts;
      fonts.regular.embedded = layout_pdf_internal::LoadPdfFontMetrics(fonts.regular, false);
      fonts.bold.embedded = layout_pdf_internal::LoadPdfFontMetrics(fonts.bold, true);
      layout_pdf_internal::PdfFontCatalog catalog{&fonts.regular, &fonts.bold};
      resources.input.style = {LegacyLegendMetrics::VectorExport, 1.45};
      const auto pdf = ComposeLegend(resources.input, [&](std::string_view text, double size, TextRole role) {
        return layout_pdf_internal::MeasureLegendPdfText(text, size, role, catalog);
      });
      assert(pdf.rows.size() + pdf.clippedRows == items.size());
      std::cout << "  vector deltas: font=" << pdf.fontSize - preview.fontSize
                << " row=" << pdf.rowHeight - preview.rowHeight
                << " Type=" << pdf.typeColumn.x - preview.typeColumn.x << '\n';
      assert(gui::RenderLegendImage({300,100}, {300,100}, 1, items, legend, nullptr).IsOk());
    }
  }
  wxTheApp->OnExit(); wxEntryCleanup();
}
