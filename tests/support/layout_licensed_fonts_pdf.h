#pragma once

#include "pdf_unicode_test_utils.h"
#include "typography/font_resources.h"
#include "pdf_objects.h"
#include "fixture_label_composition.h"
#include "canvas_text_layout.h"
#include <algorithm>
#include <iterator>
#include "viewer2dpdfexporter.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>

inline void WriteLicensedFontLayout(const std::filesystem::path &path) {
  const typography::FontCatalog catalog(PERASTAGE_TEST_RESOURCE_DIR);
  LayoutTextExportData title;
  title.frame = {12, 12, 480, 60};
  title.lines = {{{{"Producción Málaga", 20, true, false, "Source Sans 3"},
                   {" — Diseño", 20, false, false, "Noto Serif"}}}};

  LayoutTextExportData notes;
  notes.frame = {12, 75, 480, 60};
  notes.lines = {{{{"Notas: € / Ω / Ж", 14, false, false, "Noto Serif"}}},
                 {{{"Fallback de proyecto", 12, true, false, "Unverified Font"}}}};

  LayoutTextExportData shaped;
  shaped.frame = {12, 555, 480, 190};
  shaped.lines = {{{{"AV To fi ffi é é Å Å x́", 14, false, true, "Noto Serif"}}},
                  {{{"Scene 12 — שלום", 14, false, false, "Noto Sans Mono"}}},
                  {{{"Fixture 24 — مرحبا", 14, true, true, "Source Sans 3"}}},
                  {{{"España · Iluminación · À È É Ñ Ü ß € £ © ™", 14, false, false, "Noto Sans Mono"}}},
                  {{{"Αθήνα — Москва — Журнал · 🙼", 14, false, false, "Noto Sans Mono"}}},
                  {{{"A B C", 14, true, false, "Source Sans 3"}}},
                  {{{"Unsupported: 灯 😀", 14, false, false, "Noto Serif"}}}};
  LayoutLegendExportData legend;
  legend.frame = {12, 140, 480, 120};
  legend.fontFamily = "Source Sans 3";
  layouts::LegendResolvedItem row;
  row.typeName = row.displayName = "Foco Escénico";
  row.count = 3;
  row.channelCount = 17;
  legend.items = {row};

  LayoutEventTableExportData table;
  table.frame = {12, 270, 480, 120};
  table.fontFamily = "Noto Serif";
  table.fields[0] = "Teatro Español";
  table.fields[1] = "Madrid";

  LayoutViewExportData viewport;
  viewport.frame = {12, 395, 480, 140};
  viewport.viewState.viewportWidth = 480;
  viewport.viewState.viewportHeight = 140;
  LineCommand line;
  line.x0 = -1; line.y0 = -.25f; line.x1 = 1; line.y1 = .25f;
  viewport.buffer.commands.emplace_back(line);
  viewport.buffer.metadata.push_back({true, false});
  viewport.buffer.sources.push_back("geometry");
  const auto addText = [&](const TextCommand &command, const std::string &source) {
    viewport.buffer.commands.emplace_back(command);
    viewport.buffer.metadata.push_back({});
    viewport.buffer.sources.push_back(source);
  };
  const typography::FontPair fonts(PERASTAGE_TEST_RESOURCE_DIR);
  std::array<viewer2d::LabelFieldStyle, 3> fields;
  fields[0].fontFamily = "Source Sans 3";
  fields[0].size = .30f;
  fields[1].visible = fields[2].visible = false;
  Fixture fixture;
  fixture.uuid = "licensed";
  const auto addLabel = [&](const std::string &name, double x, double y) {
    fixture.instanceName = name;
    const auto glyphs = viewer2d::ComposeResolvedFixtureLabels(fields, fixture, {x,y,x,y}, fonts);
    assert(glyphs.diagnostics.empty() && glyphs.fields.front());
    for (const auto &glyph : glyphs.text) addText(glyph, "label:" + name);
    assert(fixture.instanceName == name);
    return glyphs;
  };
  addLabel("Lámpara Ágil", -2, 1.2);
  const auto he = addLabel("Rack12 שלום", 2, 1.2);
  const auto ar = addLabel("Lamp24 مرحبا", -2, 0);
  const auto marks = addLabel("x́ ffi", 2, 0);
  addLabel("مرحبا عالم", -2, -2.3);
  addLabel("שלום עולם", 2, -2.3);
  const auto resources = [&](const auto &label, const auto &family) {
    const auto id = catalog.Resolve(family, false).face->fingerprint;
    return std::any_of(label.text.begin(), label.text.end(), [&](const auto &g) {
      return g.style.fontResource == id && g.style.glyphId > 0;
    });
  };
  assert(resources(he, "Noto Sans Hebrew") && resources(ar, "Noto Sans Arabic"));
  assert(std::any_of(marks.text.begin(), marks.text.end(), [](const auto &g) {
    return g.style.clusterContinuation;
  }));
  CanvasTextStyle generic;
  generic.fontSize = .25f;
  generic.fontFamily = "Noto Serif Bold Italic";
  addText({-3, -1.2f, "Canvas Serif", generic}, "generic:serif");
  generic.fontFamily = "Source Sans 3 Italic";
  addText({1, -1.2f, "Canvas Source", generic}, "generic:source");

  generic.lineHeight = .55f;
  const auto spaced = viewer2d::ComposeCanvasText("x́\nx́", generic, 1, catalog).run;
  assert(spaced.lines.size() == 2 && std::abs(spaced.lines[1].baseline + generic.lineHeight) < 1e-6);
  for (const auto &row : spaced.lines)
    for (size_t i = row.first; i < row.first + row.count; ++i)
      assert(std::abs(spaced.glyphs[i].baseline - row.baseline - spaced.glyphs[i].offsetY) < 1e-9);
  generic.fontFamily = "perastage-sans-bold";
  const auto legacy = viewer2d::ComposeCanvasText("AV", generic, 1, catalog).run;
  assert(legacy.resourceId == catalog.Resolve({}, true).face->fingerprint);
  generic.fontFamily = "Unverified file Bold";
  const auto denied = viewer2d::ComposeCanvasText("AV", generic, 1, catalog).run;
  assert(denied.fontIssue == typography::FontIssue::Missing && denied.resourceId == legacy.resourceId);

  Viewer2DPrintOptions options;
  options.compressStreams = false;
  options.floatPrecision = 6;
  const auto result = ExportLayoutToPdf({viewport}, {legend}, {table}, {title, notes, shaped}, {}, options, path);
  assert(result.success && result.fontDiagnostics.size() == 2);
  assert(std::any_of(result.fontDiagnostics.begin(), result.fontDiagnostics.end(),
      [](const auto &warning) { return warning.find("Unverified Font") != std::string::npos; }));
  std::ifstream file(path, std::ios::binary);
  const std::string pdf{std::istreambuf_iterator<char>(file), {}};
  for (const auto name : {"SourceSans3-Bold", "SourceSans3-Regular", "NotoSerif-Regular",
                          "NotoSerif-Bold", "NotoSans-Bold"})
    assert(pdf.find(std::string("/BaseFont /") + name) != std::string::npos);
  assert(pdf.find("/WinAnsiEncoding") == std::string::npos);
  assert(pdf.find("/Helvetica") == std::string::npos);
  assert(pdf.find("/ActualText <FEFF00780301>") != std::string::npos);
  assert(pdf.find("/ActualText <FEFF006E0020004D>") != std::string::npos); // "n M"
  assert(pdf.find("/ActualText <FEFF00410020004200200043>") != std::string::npos); // "A B C"
  const auto stream = pdf_test::PageStream(pdf, true);
  const auto plan = typography::ComposeTextBlock(shaped, shaped.frame.width, shaped.frame.height, catalog);
  const layout_pdf_internal::FloatFormatter fmt(options.floatPrecision);
  for (const auto &glyph : plan.glyphs) {
    const auto x = shaped.frame.x + glyph.x;
    const auto y = options.pageHeightPt - shaped.frame.y - glyph.baseline;
    assert(stream.find(fmt.Format(x) + " " + fmt.Format(y) + " Tm") != std::string::npos);
  }
  const auto extracted = pdf_test::Text(stream);
  for (const auto text : {"Producción Málaga — Diseño", "Notas: € / Ω / Ж", "Fallback de proyecto",
                          "Foco Escénico", "Teatro Español", "Lámpara Ágil"})
    assert(extracted.find(text) != std::string::npos);
}
