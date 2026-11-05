#include "layouts/LegendComposition.h"
#include "layouts/LegendContent.h"
#include "legend_composition_adapter.h"
#include "utf8_utils.h"
#include "typography/font_resources.h"

#include <cassert>
#include <cmath>
#include <sstream>

using namespace layouts::composition;
namespace {
const typography::FontPair bundled(PERASTAGE_TEST_RESOURCE_DIR);
const LegendFonts fonts{bundled.regular, bundled.bold};
LegendCompositionInput Input(std::size_t count = 1) {
  LegendCompositionInput input;
  input.width = 300; input.height = 100;
  for (std::size_t i = 0; i < count; ++i)
    input.rows.push_back({std::to_string(i), "123", "Profile fixture", "42"});
  return input;
}
void FiniteRect(const Rect &r) {
  assert(std::isfinite(r.x) && std::isfinite(r.y));
  assert(std::isfinite(r.width) && r.width >= 0);
  assert(std::isfinite(r.height) && r.height >= 0);
}
std::string Describe(const LegendCompositionPlan &p) {
  std::ostringstream out;
  out.precision(17);
  out << p.fontSize << ' ' << p.rowHeight << ' ' << p.hiddenRows << ' ' << p.clippedRows << p.overflow;
  FiniteRect(p.clip);
  for (const auto &col : p.symbolColumns) {
    assert(std::isfinite(col.x) && col.width >= 0);
    out << col.x << ' ' << col.width;
  }
  for (const auto col : {p.countColumn, p.typeColumn, p.channelColumn}) {
    assert(std::isfinite(col.x) && col.width >= 0);
    out << col.x << ' ' << col.width;
  }
  for (const auto &row : p.rows) {
    FiniteRect(row.frame);
    out << row.id << row.sourceRow << row.frame.y << row.frame.height;
    for (const auto &line : row.nameLines) { assert(IsValidUtf8(line)); out << line << '|'; }
  }
  for (const auto &line : p.text) {
    assert(IsValidUtf8(line.text) && std::isfinite(line.baseline.y));
    out << line.text << line.top.x << line.top.y << line.baseline.x << line.baseline.y << line.width;
  }
  for (const auto &symbol : p.symbols) {
    FiniteRect(symbol.draw); FiniteRect(symbol.slot); FiniteRect(symbol.geometry.bounds);
    assert(std::isfinite(symbol.scale) && symbol.scale >= 0);
    out << symbol.geometry.reference << symbol.scale << symbol.draw.x << symbol.draw.y;
  }
  return out.str();
}
void Content() {
  std::vector<layouts::LegendSourceItem> source(3);
  source[0].typeName = "A"; source[0].count = 0;
  source[0].symbolKey = "project-token"; source[0].persistentSymbolIdentity = "exact-identity";
  source[0].gdtfPath = "fixture.gdtf"; source[0].symbolFillHex = "#12ab34";
  source[1].typeName = "B"; source[1].count = 999999; source[1].channelCount = 0;
  source[2].typeName = "C"; // Missing/mixed footprint remains absent, never guessed.
  layouts::LayoutLegendDefinition legend;
  legend.itemSettings = {{"B", true, false, false, true, "Éclairage 灯"},
                         {"A", false, true, true, false, "Hidden"},
                         {"Deleted type", true, true, true, true, ""}};
  const auto items = layouts::ResolveLegendItems(source, &legend);
  assert(items.size() == 2 && items[0].typeName == "B" && items[1].typeName == "C");
  assert(items[0].displayName == "Éclairage 灯" && items[0].channelCount == 0);
  assert(items[0].showSideSymbol && !items[0].showBottomSymbol && !items[0].showFrontSymbol);
  assert(!items[1].channelCount && items[1].showBottomSymbol && items[1].showFrontSymbol);
  const auto defaults = layouts::ResolveLegendItems(source, nullptr);
  assert(defaults[0].symbolKey == source[0].symbolKey);
  assert(defaults[0].persistentSymbolIdentity == "exact-identity");
  assert(defaults[0].gdtfPath == source[0].gdtfPath && defaults[0].symbolFillHex == source[0].symbolFillHex);
  assert(layouts::ResolveLegendItems({}, &legend).empty());
  layouts::LegendSourceItem empty; empty.typeName = "No fixtures";
  assert(layouts::ResolveLegendItems({empty}, nullptr)[0].displayName == "No fixtures");
}
void Symbols() {
  for (int mask = 0; mask < 8; ++mask) {
    auto input = Input();
    for (std::size_t view = 0; view < 3; ++view) {
      if (mask & (1 << view)) input.rows[0].symbols[view] =
          {SymbolGeometryKind::Svg, 41 + view, {3, -2, 7, 10}};
    }
    const auto plan = ComposeLegend(input, fonts);
    std::size_t expected = 0;
    for (int bit = 0; bit < 3; ++bit) expected += (mask >> bit) & 1;
    assert(plan.symbols.size() == expected);
    for (const auto &symbol : plan.symbols) {
      assert(symbol.geometry.reference == 41 + static_cast<std::size_t>(symbol.view));
      assert(symbol.geometry.bounds.x == 3 && symbol.geometry.bounds.y == -2);
      assert(symbol.slot.x + symbol.slot.width < plan.countColumn.x);
    }
    assert(Describe(plan) == Describe(ComposeLegend(input, fonts)));
  }
  SymbolDefinitionSnapshot snapshot;
  for (const auto view : {SymbolViewKind::Top, SymbolViewKind::Front, SymbolViewKind::Right}) {
    SymbolDefinition symbol;
    symbol.symbolId = static_cast<unsigned>(view) + 1;
    symbol.key = {"fallback", view, 1}; symbol.bounds = {{-2, -1}, {2, 3}};
    snapshot.emplace(symbol.symbolId, symbol);
  }
  layouts::LegendResolvedItem item; item.typeName = item.displayName = "Fallback";
  item.symbolKey = "fallback"; item.showSideSymbol = true;
  auto resolved = viewer2d::ResolveLegendComposition({item}, &snapshot, {});
  assert(resolved.symbols.size() == 3);
  assert(resolved.symbols[0].commands->key.viewKind == SymbolViewKind::Top);
  assert(resolved.symbols[2].commands->key.viewKind == SymbolViewKind::Right);
  snapshot.erase(static_cast<unsigned>(SymbolViewKind::Right) + 1);
  resolved = viewer2d::ResolveLegendComposition({item}, &snapshot, {});
  assert(!resolved.symbols[2].commands); // Side cannot borrow Bottom or Front.
  auto twoViews = Input();
  twoViews.rows[0].symbols[0] = {SymbolGeometryKind::Svg, 0, {0,0,10,10}};
  twoViews.rows[0].symbols[1] = {SymbolGeometryKind::Svg, 1, {0,0,10,10}};
  const auto legacyVector = ComposeLegend(twoViews, fonts);
  const auto front = legacyVector.symbolColumns[1];
  assert(legacyVector.countColumn.x == front.x + front.width + 6);
}
void GeometryAndText() {
  {
    for (std::size_t n : {0, 1, 20, 1000}) {
      for (double width : {0.0, 0.01, 1.0, 30.0, 120.0, 300.0, 1000.0}) {
        for (double height : {0.0, 0.01, 1.0, 100.0, 1000.0}) {
          auto input = Input(n); input.width = width; input.height = height;
          auto plan = ComposeLegend(input, fonts);
          assert(plan.rows.size() + plan.clippedRows == n);
          assert(Describe(plan) == Describe(ComposeLegend(input, fonts)));
        }
      }
    }
  }
  auto input = Input(2);
  input.rows[0].visible = false;
  auto p = ComposeLegend(input, fonts);
  assert(p.hiddenRows == 1 && p.rows.size() == 1 && p.rows[0].id == "1");
  input = Input();
  p = ComposeLegend(input, fonts);
  // Canonical document geometry, independent of rendering target.
  assert(p.rowHeight == 49 && p.countColumn.x == 6);
  assert(p.fontSize > 0 && p.typeColumn.x > p.countColumn.x);
  assert(p.rows[0].nameLines[0] == "Profile fixture");
  input.showChannelColumn = false;
  p = ComposeLegend(input, fonts);
  assert(p.channelColumn.width == 0 && p.channelColumn.x == input.width);
  assert(p.text.size() == 4);
  input.width = 65;
  for (const std::string name : {"Very long fixture name without truncating its remainder",
      "ÉclairageSüd灯😀日本語", "ASCIIabcdefghijklmno"}) {
    input.rows[0].name = name;
    p = ComposeLegend(input, fonts);
    assert(!p.rows[0].nameLines[0].empty() && !p.rows[0].nameLines[1].empty());
    assert(IsValidUtf8(p.rows[0].nameLines[0]) && IsValidUtf8(p.rows[0].nameLines[1]));
    if (name.starts_with("Very long")) assert(p.rows[0].overflow);
  }
  input.width = 300;
  input.rows[0].name.clear();
  for (int i=0;i<100;++i) input.rows[0].name += "灯";
  p = ComposeLegend(input, fonts);
  assert(p.unsupportedGlyphs == 100 && p.diagnostics.size() == 16);
  input.width = 1;
  p = ComposeLegend(input, fonts);
  assert(p.typeColumn.width == 0 && p.rows[0].overflow);
  input = Input(); input.rows[0].name = "No fixtures"; input.rows[0].count = "0";
  assert(ComposeLegend(input, fonts).rows[0].nameLines[0] == "No fixtures");
}
}
int main() { Content(); Symbols(); GeometryAndText(); }
