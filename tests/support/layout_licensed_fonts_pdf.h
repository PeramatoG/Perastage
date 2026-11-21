#pragma once

#include "pdf_unicode_test_utils.h"
#include "typography/font_resources.h"
#include <iterator>
#include "viewer2dpdfexporter.h"
#include <cassert>
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
  const auto label = typography::Compose("Lámpara Ágil", {.36, typography::TextUnit::SceneMetres, false},
                                        catalog, "Source Sans 3");
  for (const auto &glyph : label.glyphs) {
    CanvasTextStyle style;
    style.fontResource = glyph.resourceId;
    style.fontSize = label.style.size;
    style.glyphId = glyph.glyphId;
    style.codepoint = glyph.codepoint;
    style.outlineWidth = .004f;
    style.outlineColor = {1, 1, 1, 1};
    viewport.buffer.commands.emplace_back(TextCommand{
        static_cast<float>(glyph.x), static_cast<float>(glyph.baseline), glyph.text, style});
    viewport.buffer.metadata.push_back({});
    viewport.buffer.sources.push_back("label:fixture");
  }

  Viewer2DPrintOptions options;
  options.compressStreams = false;
  options.floatPrecision = 6;
  const auto result = ExportLayoutToPdf({viewport}, {legend}, {table}, {title, notes}, {}, options, path);
  assert(result.success && result.fontDiagnostics.size() == 1);
  assert(result.fontDiagnostics.front().find("Unverified Font") != std::string::npos);
  std::ifstream file(path, std::ios::binary);
  const std::string pdf{std::istreambuf_iterator<char>(file), {}};
  for (const auto name : {"SourceSans3-Bold", "SourceSans3-Regular", "NotoSerif-Regular",
                          "NotoSerif-Bold", "NotoSans-Bold"})
    assert(pdf.find(std::string("/BaseFont /") + name) != std::string::npos);
  assert(pdf.find("/WinAnsiEncoding") == std::string::npos);
  assert(pdf.find("/Helvetica") == std::string::npos);
  const auto extracted = pdf_test::Text(pdf_test::PageStream(pdf, true));
  for (const auto text : {"Producción Málaga — Diseño", "Notas: € / Ω / Ж", "Fallback de proyecto",
                          "Foco Escénico", "Teatro Español", "Lámpara Ágil"})
    assert(extracted.find(text) != std::string::npos);
}
