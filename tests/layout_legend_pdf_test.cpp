#include "viewer2dpdfexporter.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace {
std::string ExportPage(const std::vector<LayoutViewExportData> &views,
    const std::vector<LayoutLegendExportData> &legends,
    const std::vector<LayoutEventTableExportData> &tables,
    const std::vector<LayoutTextExportData> &texts,
    const std::vector<LayoutImageExportData> &images, Viewer2DExportResult &result) {
  const auto path = std::filesystem::temp_directory_path() /
      ("legend-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".pdf");
  Viewer2DPrintOptions options;
  options.compressStreams = false;
  result = ExportLayoutToPdf(views, legends, tables, texts, images, options, path);
  assert(result.success);
  std::ifstream file(path, std::ios::binary);
  std::ostringstream bytes;
  bytes << file.rdbuf();
  file.close();
  std::filesystem::remove(path);
  return bytes.str();
}
std::string Export(const LayoutLegendExportData &legend, Viewer2DExportResult *result = nullptr) {
  Viewer2DExportResult actual;
  auto pdf = ExportPage({}, {legend}, {}, {}, {}, actual);
  if (result) *result = actual;
  // The page content object follows fonts and symbol/image definitions.
  const auto start = pdf.rfind("stream\n", pdf.find("(Count) Tj"));
  return pdf.substr(start + 7, pdf.find("endstream", start) - start - 7);
}
std::size_t Count(const std::string &text, const std::string &part) {
  std::size_t count = 0;
  for (std::size_t pos = 0; (pos = text.find(part, pos)) != std::string::npos; pos += part.size()) ++count;
  return count;
}
std::vector<std::string> DrawingGroups(const std::string &pdf) {
  const auto marker = pdf.find("(Count) Tj");
  const auto start = pdf.rfind("stream\n", marker) + 7;
  const auto stream = pdf.substr(start, pdf.find("endstream", start) - start);
  std::istringstream lines(stream);
  std::vector<std::string> groups;
  std::string line, group;
  int depth = 0;
  while (std::getline(lines, line)) {
    if (line == "q") ++depth;
    group += line + '\n';
    if (line == "Q") --depth;
    if (depth == 0) { groups.push_back(group); group.clear(); }
  }
  return groups;
}
void MixedPage(LayoutLegendExportData legend) {
  legend.items[0].displayName = "ALabel"; legend.zIndex = 1;
  auto second = legend; second.zIndex = 3; second.items[0].displayName = "BLabel";
  second.showChannelColumn = false;
  LayoutViewExportData view; view.frame = legend.frame; view.zIndex = 0;
  view.viewState.viewportWidth = 300; view.viewState.viewportHeight = 100;
  LineCommand line; line.x1 = 1; line.y1 = 1;
  view.buffer.commands.emplace_back(line); view.buffer.metadata.push_back({true,false});
  view.buffer.sources.push_back("fixture");
  LayoutEventTableExportData table; table.frame = legend.frame; table.zIndex = 2;
  table.fields[0] = "Venue test";
  auto secondTable = table; secondTable.fields[0] = "Second venue"; secondTable.zIndex = 6;
  LayoutTextExportData text; text.frame = legend.frame; text.zIndex = 4;
  text.lines = {{{{"Overlay",12,true,false},{" rich text",12,false,true}}}};
  LayoutImageExportData image; image.frame = legend.frame; image.zIndex = 5;
  image.pixelWidth = image.pixelHeight = 1; image.pixels = {255,0,0};
  Viewer2DExportResult result;
  auto before = ExportPage({view}, {legend,second}, {table,secondTable}, {text}, {image}, result);
  auto groups = DrawingGroups(before);
  assert(before.find("/Subtype /Image") != std::string::npos);
  assert(before.find("(ALabel) Tj") < before.find("(Venue:) Tj"));
  assert(before.find("(Venue:) Tj") < before.find("(BLabel) Tj"));
  assert(before.find("(BLabel) Tj") < before.find("(Overlay) Tj"));
  const auto imageGroup = std::find_if(groups.begin(), groups.end(), [](const auto &g) { return g.find(" Do\n") != std::string::npos; });
  assert(imageGroup != groups.end());
  table.fields[0] = "Edited venue";
  auto tableEdit = DrawingGroups(ExportPage({view}, {legend,second}, {table,secondTable}, {text}, {image}, result));
  for (const auto &group : groups) {
    if (group.find("(Venue test) Tj") != std::string::npos) continue;
    assert(std::find(tableEdit.begin(), tableEdit.end(), group) != tableEdit.end());
  }
  table.fields[0] = "Venue test";
  legend.items[0].showSideSymbol = true;
  legend.items[0].displayName = "Changed A";
  const auto after = ExportPage({view}, {legend,second}, {table,secondTable}, {text}, {image}, result);
  const auto afterGroups = DrawingGroups(after);
  // Every other placed element's complete drawing groups are byte-identical.
  for (const auto &group : groups) {
    if (group.find("(ALabel) Tj") != std::string::npos) continue;
    assert(std::find(afterGroups.begin(), afterGroups.end(), group) != afterGroups.end());
  }
}
}

int main(int argc, char **argv) {
  LayoutLegendExportData legend;
  legend.frame = {10, 20, 300, 100};
  LayoutLegendItem item;
  item.typeName = item.displayName = "Profile fixture";
  item.count = 123;
  item.channelCount = 42;
  item.symbolKey = "legend-test.gdtf";
  legend.items.push_back(item);
  Viewer2DExportResult structureResult;
  const auto document=ExportPage({}, {legend}, {}, {}, {}, structureResult);
  assert(document.find("/FontFile2") != std::string::npos);
  assert(document.find("/F3 ") != std::string::npos && document.find("/F4 ") != std::string::npos);
  assert(document.find("/BaseFont /NotoSans") != std::string::npos);
  const auto baseline = Export(legend);
  assert(baseline.find("(Count) Tj") != std::string::npos);
  assert(baseline.find("(Profile fixture) Tj") != std::string::npos);
  assert(baseline.find("(123) Tj") != std::string::npos);
  assert(baseline.find("(42) Tj") != std::string::npos);
  assert(baseline.find(" re W n") != std::string::npos);
  assert(baseline.find("/Subtype /Image") == std::string::npos);
  legend.showChannelColumn = false;
  assert(Export(legend).find("(Ch) Tj") == std::string::npos);
  legend.showChannelColumn = true;
  legend.items[0].showSideSymbol = true;
  const auto side = Export(legend);
  assert(side != baseline && "Enabling Side must change the vector page stream");
  for (int mask = 0; mask < 8; ++mask) {
    auto &row = legend.items[0];
    row.showBottomSymbol = (mask & 1) != 0;
    row.showFrontSymbol = (mask & 2) != 0;
    row.showSideSymbol = (mask & 4) != 0;
    const std::size_t expected = bool(mask & 1) + bool(mask & 2) + bool(mask & 4);
    Viewer2DExportResult result;
    const auto stream = Export(legend, &result);
    assert(result.perastageSymbolInstances == expected && result.fallbackSymbolInstances == 0);
    assert(Count(stream, "h\nf\n") == expected); // Actual vector paths, not only changed text positions.
    assert(stream.find("/Subtype /Image") == std::string::npos);
  }
  auto &row = legend.items[0];
  row.showBottomSymbol = row.showFrontSymbol = row.showSideSymbol = true;
  row.displayName = "É灯😀";
  Viewer2DExportResult result;
  const auto unicode = Export(legend, &result);
  assert(result.legendTextSubstitutions == 2);
  assert(unicode.find("(\\311) Tj") != std::string::npos);
  assert(result.legendTextDiagnostics.size() == 2);
  assert(result.legendTextDiagnostics[0].find("byte") != std::string::npos);
  assert(row.displayName == "É灯😀");
  row.displayName = "Profile fixture";
  row.symbolKey = "pstg-symbol:test:content";
  assert(Count(Export(legend), "h\nf\n") == 3);
  row.symbolKey = "unrelated"; row.gdtfPath = "legend-test.gdtf";
  assert(Count(Export(legend), "h\nf\n") == 3);
  row.gdtfPath.clear();
  Export(legend, &result); assert(result.perastageSymbolInstances == 0 && result.fallbackSymbolInstances == 0);
  row.symbolKey = "fallback";
  auto snapshot = std::make_shared<SymbolDefinitionSnapshot>();
  for (auto view : {SymbolViewKind::Bottom, SymbolViewKind::Front, SymbolViewKind::Right}) {
    SymbolDefinition symbol; symbol.symbolId = static_cast<unsigned>(view) + 1;
    symbol.key = {"fallback",view,1}; symbol.bounds = {{-.5f,-.5f},{.5f,.5f}};
    PolygonCommand polygon; polygon.points = {-.5f,-.5f,.5f,-.5f,0,.5f}; polygon.hasFill = true;
    symbol.localCommands.commands.emplace_back(polygon);
    symbol.localCommands.metadata.push_back({false,true}); symbol.localCommands.sources.push_back("fixture");
    snapshot->emplace(symbol.symbolId, symbol);
  }
  legend.symbolSnapshot = snapshot;
  const auto fallback = Export(legend, &result);
  assert(result.fallbackSymbolInstances == 3 && result.perastageSymbolInstances == 0);
  assert(fallback.find(" l") != std::string::npos);
  snapshot->erase(static_cast<unsigned>(SymbolViewKind::Right) + 1);
  Export(legend, &result); assert(result.fallbackSymbolInstances == 2);
  legend.symbolSnapshot.reset(); row.symbolKey = "legend-test.gdtf";
  MixedPage(legend);
  if (argc > 1) {
    Viewer2DPrintOptions options;
    options.compressStreams = false;
    std::vector<std::string> names={"Exterior Linear-1200 Graze","MAC Ultra Performance","Martin Atomic 3000 LED"};
    auto sample=legend; sample.items.resize(3,legend.items[0]); sample.frame={0,0,300,100};
    for (std::size_t i=0;i<names.size();++i) sample.items[i].typeName=sample.items[i].displayName=names[i];
    for (int width : {120,180,300,600}) {
      sample.frame.width=width;
      for (double scale : {0.5,1.0,2.0}) {
        options.layoutScaleX=options.layoutScaleY=scale;
        options.pageWidthPt=width*scale; options.pageHeightPt=100*scale;
        assert(ExportLayoutToPdf({}, {sample}, {}, {}, {}, options,
            std::filesystem::path(argv[1]) / ("parity-"+std::to_string(width)+"-"+std::to_string(scale)+".pdf")).success);
      }
    }
    options=Viewer2DPrintOptions{}; options.compressStreams=false;
    assert(ExportLayoutToPdf({}, {legend}, {}, {}, {}, options,
        std::filesystem::path(argv[1]) / "legend-three-views.pdf").success);
  }

  if (argc > 2 && std::string(argv[2]) == "event-table") {
    LayoutEventTableExportData table;
    table.fields = {"Venue Éclairage", "Málaga", "2026-11-05", "Main stage", "Version 1", "Design", "mail@example.org"};
    for (int width : {120,180,300,600}) for (double scale : {0.5,1.,2.}) {
      table.frame = {0,0,width,100};
      Viewer2DPrintOptions options; options.compressStreams = false;
      options.layoutScaleX = options.layoutScaleY = scale;
      options.pageWidthPt = width*scale; options.pageHeightPt = 100*scale;
      assert(ExportLayoutToPdf({}, {}, {table}, {}, {}, options,
          std::filesystem::path(argv[1]) / ("parity-"+std::to_string(width)+"-"+std::to_string(scale)+".pdf")).success);
    }
    Viewer2DExportResult result;
    const auto pdf = ExportPage({}, {}, {table}, {}, {}, result);
    assert(pdf.find("/FontFile2") != std::string::npos);
    assert(pdf.find("/F3 ") != std::string::npos && pdf.find("/F4 ") != std::string::npos);
    assert(pdf.find("/Subtype /Image") == std::string::npos);
    assert(pdf.find("(Venue:) Tj") < pdf.find("(Location:) Tj"));
    table.fields[0] = "É灯😀";
    ExportPage({}, {}, {table}, {}, {}, result);
    assert(result.eventTableTextSubstitutions == 2 && result.eventTableTextDiagnostics.size() == 2);
  }
  legend.items.clear();
  assert(Export(legend).find("(Count) Tj") != std::string::npos);
}
