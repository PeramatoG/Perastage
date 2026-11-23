#pragma once

#include "fixture_label_composition.h"
#include "pdf_unicode_test_utils.h"
#include "system_font_test_utils.h"
#include "viewer2dpdfexporter.h"
#include <algorithm>
#include <iostream>
#include <iterator>

inline void WriteSystemFontLayout(const std::filesystem::path &path) {
  using namespace system_font_test;
  using namespace typography;
  Files files;
  auto restricted = Sans();
  restricted.family = "Restricted test face";
  restricted.path = files.ChangeEmbedding("restricted.ttf", 2);
  auto editable = Sans();
  editable.family = "Editable test face";
  editable.path = files.ChangeEmbedding("editable-full.ttf", 0x108);
  auto print = Sans();
  print.family = "Preview test face";
  print.path = files.ChangeEmbedding("preview.ttf", 4);
  const auto catalog = std::make_shared<const FontCatalog>(PERASTAGE_TEST_RESOURCE_DIR,
      std::vector<std::filesystem::path>{},
      std::vector{Sans(), Cff(), BoldCollection(), restricted, editable, print});
  const auto restrictedReference = catalog->ReferenceForFamily(restricted.family);
  const auto cffReference = catalog->ReferenceForFamily(adobeStyle);
  const auto availableReference = catalog->ReferenceForFamily(sans);
  LayoutTextExportData notes;
  notes.frame = {12, 12, 480, 120};
  notes.lines = {{{{"External café", 18, false, false, availableReference}}},
                 {{{"Restricted x́ ffi", 18, false, false, restrictedReference}}},
                 {{{"CFF fallback", 18, false, false, cffReference}}},
                 {{{"Print fallback", 18, false, false, print.family}}},
                 {{{"Editable full", 18, false, false, editable.family}}},
                 {{{"Collection fallback", 18, true, false, availableReference}}}};

  LayoutEventTableExportData table;
  table.frame = {12, 137, 480, 150};
  table.fields[0] = "External event";
  table.fontFamily = availableReference;

  LayoutLegendExportData legend;
  legend.frame = {12, 292, 480, 80};
  legend.fontFamily = restrictedReference;
  layouts::LegendResolvedItem row;
  row.typeName = row.displayName = "External legend";
  row.count = 2;
  legend.items = {row};

  LayoutViewExportData view;
  view.frame = {12, 382, 480, 170};
  view.viewState.viewportWidth = 480;
  view.viewState.viewportHeight = 170;
  Fixture fixture;
  fixture.uuid = "system fixture";
  fixture.instanceName = "Gate12 مرحبا";
  std::array<viewer2d::LabelFieldStyle, 3> fields;
  fields[0].fontFamily = restrictedReference;
  fields[0].size = .35f;
  fields[1].visible = fields[2].visible = false;
  const FontPair local(catalog);
  const auto label = viewer2d::ComposeResolvedFixtureLabels(fields, fixture, {-2, 0, -2, 0}, local);
  const auto localRestricted = catalog->Resolve(restrictedReference, false).face;
  assert(localRestricted && localRestricted->fingerprint == catalog->Resolve(restricted.family, false).face->fingerprint);
  assert(label.fields[0] && label.text[0].style.source && label.text[0].style.source->text == fixture.instanceName);
  assert(label.text[0].style.fontResource == localRestricted->fingerprint);
  for (const auto &glyph : label.text) {
    view.buffer.commands.emplace_back(glyph);
    view.buffer.metadata.push_back({});
    view.buffer.sources.push_back(fixture.uuid);
  }
  CanvasTextStyle generic;
  generic.fontFamily = adobeStyle;
  generic.fontSize = .25f;
  view.buffer.commands.emplace_back(TextCommand{0, -1, "Canvas CFF", generic});
  view.buffer.metadata.push_back({});
  view.buffer.sources.push_back("generic");

  Viewer2DPrintOptions options;
  options.compressStreams = false;
  options.fontCatalog = catalog;
  const auto started = std::chrono::steady_clock::now();
  const auto result = ExportLayoutToPdf({view}, {legend}, {table}, {notes}, {}, options, path);
  std::cout << "Mixed-font PDF export: " <<
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count() << " ms\n";
  assert(result.success);
  for (const std::string_view family : {std::string_view(restricted.family), std::string_view(adobeStyle),
                                       std::string_view(print.family)}) {
    assert(std::count_if(result.fontDiagnostics.begin(), result.fontDiagnostics.end(), [&](const auto &warning) {
      return warning.starts_with(family);
    }) == 1);
  }
  std::ifstream file(path, std::ios::binary);
  const std::string pdf{std::istreambuf_iterator<char>(file), {}};
  assert(pdf.find("/BaseFont /PerastageTestSans-Regular") != std::string::npos);
  assert(pdf.find("/BaseFont /PerastageTestCff-Regular") == std::string::npos);
  assert(pdf.find("/BaseFont /PerastageTestSans-Bold") == std::string::npos);
  const auto stream = pdf_test::PageStream(pdf, true);
  const auto output = pdf_test::Text(stream);
  assert(output.find("External café") != std::string::npos);
  assert(output.find("Restricted x́ ffi") != std::string::npos);
  assert(output.find("Gate12") != std::string::npos);
  assert(label.text[0].style.source->family == restrictedReference);
}
