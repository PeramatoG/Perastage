#include "layoutlegendcachepolicy.h"
#include "layouts/LayoutTemplateSerializer.h"
#include "layouts/LegendContent.h"

#include <cassert>
#include <unordered_map>

int main() {
  layouts::LayoutDefinition page;
  page.name = "Two independent legends";
  layouts::LayoutLegendDefinition a, b;
  a.id = 1; b.id = 2;
  a.itemSettings = {{"Spot", true, true, false, true, "Side"}};
  b.itemSettings = {{"Wash", false, true, true, false, "Hidden"}};
  page.legendViews = {a, b};
  layouts::LayoutTextDefinition overlapping;
  overlapping.id = 3; overlapping.zIndex = 10; overlapping.text = "Overlap";
  page.textViews.push_back(overlapping);
  struct Cache { std::size_t contentHash; bool renderDirty = false; };
  // Use the persisted semantic value as a test fingerprint, independent of
  // selection, frame raster resolution, or an implementation's hash algorithm.
  auto hash = [](const layouts::LayoutLegendDefinition &legend) {
    layouts::LayoutDefinition single; single.legendViews = {legend};
    return std::hash<std::string>{}(layouts::ToJson(single).dump());
  };
  std::unordered_map<int, Cache> caches{{1, {hash(a)}}, {2, {hash(b)}}};
  auto refresh = [&] { return gui::InvalidateChangedLegendRasters(page.legendViews, caches, hash); };
  assert(!refresh()); // Selection-only refresh, including selecting B, reuses A.
  const auto original = layouts::ToJson(page);
  page.legendViews[0].showChannelColumn = false;
  page.legendViews[0].itemSettings[0].showSideSymbol = false;
  assert(refresh() && caches[1].renderDirty && !caches[2].renderDirty);
  const auto edited = layouts::ToJson(page);
  caches[1] = {hash(page.legendViews[0])};
  assert(!refresh());
  page.legendViews[1].itemSettings[0].visible = true;
  assert(refresh() && !caches[1].renderDirty && caches[2].renderDirty);
  std::string error;
  // Exercise the same states delivered to SetLayout by Undo/Redo and reload.
  assert(layouts::FromJson(original, page, &error));
  caches[2] = {hash(page.legendViews[1])};
  assert(refresh() && caches[1].renderDirty && !caches[2].renderDirty);
  caches[1] = {hash(page.legendViews[0])};
  assert(layouts::FromJson(edited, page, &error));
  assert(refresh() && caches[1].renderDirty && !caches[2].renderDirty);
  caches[1] = {hash(page.legendViews[0])};
  auto saved = layouts::ToTemplateDocument({page});
  std::vector<layouts::LayoutDefinition> reopened;
  assert(layouts::FromTemplateDocument(saved, reopened, &error));
  page = reopened.front();
  assert(!refresh() && layouts::ToTemplateDocument(reopened) == saved);
  assert(page.textViews[0].text == "Overlap" && page.textViews[0].zIndex == 10);
}
