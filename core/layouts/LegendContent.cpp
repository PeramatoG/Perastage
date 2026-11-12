#include "LegendContent.h"

#include <unordered_map>
#include <unordered_set>

namespace layouts {
std::vector<LegendResolvedItem> ResolveLegendItems(
    const std::vector<LegendSourceItem> &source,
    const LayoutLegendDefinition *legend) {
  std::unordered_map<std::string, LayoutLegendDefinition::ItemSettings> settingsByType;
  if (legend) {
    for (const auto &settings : legend->itemSettings)
      settingsByType[settings.typeName] = settings;
  }
  std::unordered_map<std::string, const LegendSourceItem *> availableByType;
  for (const auto &item : source) availableByType.emplace(item.typeName, &item);
  std::vector<LegendResolvedItem> items;
  items.reserve(source.size());
  auto append = [&](const LegendSourceItem &shared) {
    LegendResolvedItem item;
    static_cast<LegendSourceItem &>(item) = shared;
    item.displayName = shared.typeName;
    if (const auto it = settingsByType.find(shared.typeName); it != settingsByType.end()) {
      item.showBottomSymbol = it->second.showBottomSymbol;
      item.showFrontSymbol = it->second.showFrontSymbol;
      item.showSideSymbol = it->second.showSideSymbol;
      if (!it->second.customName.empty()) item.displayName = it->second.customName;
      if (!it->second.visible) return;
    }
    items.push_back(std::move(item));
  };
  std::unordered_set<std::string> used;
  if (legend) {
    for (const auto &settings : legend->itemSettings) {
      if (const auto it = availableByType.find(settings.typeName); it != availableByType.end()) {
        append(*it->second);
        used.insert(settings.typeName);
      }
    }
  }
  for (const auto &shared : source)
    if (!used.count(shared.typeName)) append(shared);
  return items;
}
} // namespace layouts
