#pragma once
#include "layouts/EventTableComposition.h"
#include "layouts/LayoutCollection.h"
#include <functional>
namespace gui {
// Position and selection do not alter the raster; each table owns its own cache.
inline std::size_t EventTableContentHash(const layouts::LayoutEventTableDefinition &table) {
  std::size_t hash = std::hash<std::string>{}(layouts::composition::kEventTableCompositionVersion);
  auto mix = [&](std::size_t value) { hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2); };
  mix(std::hash<int>{}(table.frame.width)); mix(std::hash<int>{}(table.frame.height));
  for (const auto &value : table.fields) mix(std::hash<std::string>{}(value));
  return hash;
}
}
