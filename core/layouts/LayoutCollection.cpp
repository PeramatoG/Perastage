/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "LayoutCollection.h"

#include <algorithm>

namespace layouts {
namespace {
int MaxZIndex(const LayoutDefinition &layout) {
  bool hasValue = false;
  int maxZ = 0;
  for (const auto &view : layout.view2dViews) {
    if (!hasValue) {
      maxZ = view.zIndex;
      hasValue = true;
    } else {
      maxZ = std::max(maxZ, view.zIndex);
    }
  }
  for (const auto &legend : layout.legendViews) {
    if (!hasValue) {
      maxZ = legend.zIndex;
      hasValue = true;
    } else {
      maxZ = std::max(maxZ, legend.zIndex);
    }
  }
  for (const auto &table : layout.eventTables) {
    if (!hasValue) {
      maxZ = table.zIndex;
      hasValue = true;
    } else {
      maxZ = std::max(maxZ, table.zIndex);
    }
  }
  for (const auto &text : layout.textViews) {
    if (!hasValue) {
      maxZ = text.zIndex;
      hasValue = true;
    } else {
      maxZ = std::max(maxZ, text.zIndex);
    }
  }
  return hasValue ? maxZ : 0;
}
} // namespace

LayoutCollection::LayoutCollection() { layouts.push_back(DefaultLayout()); }

const std::vector<LayoutDefinition> &LayoutCollection::Items() const {
  return layouts;
}

std::size_t LayoutCollection::Count() const { return layouts.size(); }

bool LayoutCollection::AddLayout(const LayoutDefinition &layout) {
  if (layout.name.empty() || NameExists(layout.name))
    return false;
  layouts.push_back(layout);
  return true;
}

bool LayoutCollection::RenameLayout(const std::string &currentName,
                                    const std::string &newName) {
  if (newName.empty())
    return false;
  if (NameExists(newName, currentName))
    return false;
  for (auto &layout : layouts) {
    if (layout.name == currentName) {
      layout.name = newName;
      return true;
    }
  }
  return false;
}

bool LayoutCollection::RemoveLayout(const std::string &name) {
  if (layouts.size() <= 1)
    return false;
  for (auto it = layouts.begin(); it != layouts.end(); ++it) {
    if (it->name == name) {
      layouts.erase(it);
      return true;
    }
  }
  return false;
}

bool LayoutCollection::SetLayoutOrientation(const std::string &name,
                                            bool landscape) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      layout.pageSetup.landscape = landscape;
      return true;
    }
  }
  return false;
}

bool LayoutCollection::UpdateLayout2DView(const std::string &name,
                                          const Layout2DViewDefinition &view) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      Layout2DViewDefinition updatedView = view;
      int nextId = 1;
      for (const auto &entry : layout.view2dViews) {
        if (entry.id > 0)
          nextId = std::max(nextId, entry.id + 1);
      }
      if (updatedView.id <= 0)
        updatedView.id = nextId;
      bool replaced = false;
      for (auto &entry : layout.view2dViews) {
        if (entry.id == updatedView.id && updatedView.id > 0) {
          if (updatedView.zIndex == 0 && entry.zIndex != 0)
            updatedView.zIndex = entry.zIndex;
          entry = updatedView;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        if (updatedView.zIndex == 0 &&
            (!layout.view2dViews.empty() || !layout.legendViews.empty() ||
             !layout.eventTables.empty() || !layout.textViews.empty())) {
          updatedView.zIndex = MaxZIndex(layout) + 1;
        }
        layout.view2dViews.push_back(updatedView);
      }
      return true;
    }
  }
  return false;
}

bool LayoutCollection::RemoveLayout2DView(const std::string &name,
                                          int viewId) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      auto &views = layout.view2dViews;
      auto it = std::remove_if(
          views.begin(), views.end(), [viewId](const auto &entry) {
            return entry.id == viewId;
          });
      if (it == views.end())
        return false;
      views.erase(it, views.end());
      return true;
    }
  }
  return false;
}

bool LayoutCollection::MoveLayout2DView(const std::string &name, int viewId,
                                        bool toFront) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      auto &views = layout.view2dViews;
      auto it = std::find_if(views.begin(), views.end(),
                             [viewId](const auto &entry) {
                               return entry.id == viewId;
                             });
      if (it == views.end())
        return false;
      if (toFront) {
        if (std::next(it) != views.end()) {
          auto moved = std::move(*it);
          views.erase(it);
          views.push_back(std::move(moved));
        }
      } else {
        if (it != views.begin()) {
          auto moved = std::move(*it);
          views.erase(it);
          views.insert(views.begin(), std::move(moved));
        }
      }
      return true;
    }
  }
  return false;
}

bool LayoutCollection::UpdateLayoutLegend(
    const std::string &name, const LayoutLegendDefinition &legend) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      LayoutLegendDefinition updatedLegend = legend;
      int nextId = 1;
      for (const auto &entry : layout.legendViews) {
        if (entry.id > 0)
          nextId = std::max(nextId, entry.id + 1);
      }
      if (updatedLegend.id <= 0)
        updatedLegend.id = nextId;
      bool replaced = false;
      for (auto &entry : layout.legendViews) {
        if (entry.id == updatedLegend.id && updatedLegend.id > 0) {
          if (updatedLegend.zIndex == 0 && entry.zIndex != 0)
            updatedLegend.zIndex = entry.zIndex;
          entry = updatedLegend;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        if (updatedLegend.zIndex == 0 &&
            (!layout.view2dViews.empty() || !layout.legendViews.empty() ||
             !layout.eventTables.empty() || !layout.textViews.empty())) {
          updatedLegend.zIndex = MaxZIndex(layout) + 1;
        }
        layout.legendViews.push_back(updatedLegend);
      }
      return true;
    }
  }
  return false;
}

bool LayoutCollection::UpdateLayoutEventTable(
    const std::string &name, const LayoutEventTableDefinition &table) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      LayoutEventTableDefinition updatedTable = table;
      int nextId = 1;
      for (const auto &entry : layout.eventTables) {
        if (entry.id > 0)
          nextId = std::max(nextId, entry.id + 1);
      }
      if (updatedTable.id <= 0)
        updatedTable.id = nextId;
      bool replaced = false;
      for (auto &entry : layout.eventTables) {
        if (entry.id == updatedTable.id && updatedTable.id > 0) {
          if (updatedTable.zIndex == 0 && entry.zIndex != 0)
            updatedTable.zIndex = entry.zIndex;
          entry = updatedTable;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        if (updatedTable.zIndex == 0 &&
            (!layout.view2dViews.empty() || !layout.legendViews.empty() ||
             !layout.eventTables.empty() || !layout.textViews.empty())) {
          updatedTable.zIndex = MaxZIndex(layout) + 1;
        }
        layout.eventTables.push_back(updatedTable);
      }
      return true;
    }
  }
  return false;
}

bool LayoutCollection::UpdateLayoutText(const std::string &name,
                                        const LayoutTextDefinition &text) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      LayoutTextDefinition updatedText = text;
      int nextId = 1;
      for (const auto &entry : layout.textViews) {
        if (entry.id > 0)
          nextId = std::max(nextId, entry.id + 1);
      }
      if (updatedText.id <= 0)
        updatedText.id = nextId;
      bool replaced = false;
      for (auto &entry : layout.textViews) {
        if (entry.id == updatedText.id && updatedText.id > 0) {
          if (updatedText.zIndex == 0 && entry.zIndex != 0)
            updatedText.zIndex = entry.zIndex;
          entry = updatedText;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        if (updatedText.zIndex == 0 &&
            (!layout.view2dViews.empty() || !layout.legendViews.empty() ||
             !layout.eventTables.empty() || !layout.textViews.empty())) {
          updatedText.zIndex = MaxZIndex(layout) + 1;
        }
        layout.textViews.push_back(updatedText);
      }
      return true;
    }
  }
  return false;
}

bool LayoutCollection::RemoveLayoutLegend(const std::string &name,
                                          int legendId) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      auto &legends = layout.legendViews;
      auto it =
          std::remove_if(legends.begin(), legends.end(),
                         [legendId](const auto &entry) {
                           return entry.id == legendId;
                         });
      if (it == legends.end())
        return false;
      legends.erase(it, legends.end());
      return true;
    }
  }
  return false;
}

bool LayoutCollection::RemoveLayoutEventTable(const std::string &name,
                                              int tableId) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      auto &tables = layout.eventTables;
      auto it =
          std::remove_if(tables.begin(), tables.end(),
                         [tableId](const auto &entry) {
                           return entry.id == tableId;
                         });
      if (it == tables.end())
        return false;
      tables.erase(it, tables.end());
      return true;
    }
  }
  return false;
}

bool LayoutCollection::RemoveLayoutText(const std::string &name, int textId) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      auto &texts = layout.textViews;
      auto it =
          std::remove_if(texts.begin(), texts.end(),
                         [textId](const auto &entry) {
                           return entry.id == textId;
                         });
      if (it == texts.end())
        return false;
      texts.erase(it, texts.end());
      return true;
    }
  }
  return false;
}

bool LayoutCollection::MoveLayoutLegend(const std::string &name, int legendId,
                                        bool toFront) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      auto &legends = layout.legendViews;
      auto it = std::find_if(legends.begin(), legends.end(),
                             [legendId](const auto &entry) {
                               return entry.id == legendId;
                             });
      if (it == legends.end())
        return false;
      if (toFront) {
        if (std::next(it) != legends.end()) {
          auto moved = std::move(*it);
          legends.erase(it);
          legends.push_back(std::move(moved));
        }
      } else {
        if (it != legends.begin()) {
          auto moved = std::move(*it);
          legends.erase(it);
          legends.insert(legends.begin(), std::move(moved));
        }
      }
      return true;
    }
  }
  return false;
}

bool LayoutCollection::MoveLayoutText(const std::string &name, int textId,
                                      bool toFront) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      auto &texts = layout.textViews;
      auto it = std::find_if(texts.begin(), texts.end(),
                             [textId](const auto &entry) {
                               return entry.id == textId;
                             });
      if (it == texts.end())
        return false;
      if (toFront) {
        if (std::next(it) != texts.end()) {
          auto moved = std::move(*it);
          texts.erase(it);
          texts.push_back(std::move(moved));
        }
      } else {
        if (it != texts.begin()) {
          auto moved = std::move(*it);
          texts.erase(it);
          texts.insert(texts.begin(), std::move(moved));
        }
      }
      return true;
    }
  }
  return false;
}

bool LayoutCollection::MoveLayoutEventTable(const std::string &name, int tableId,
                                            bool toFront) {
  for (auto &layout : layouts) {
    if (layout.name == name) {
      auto &tables = layout.eventTables;
      auto it = std::find_if(tables.begin(), tables.end(),
                             [tableId](const auto &entry) {
                               return entry.id == tableId;
                             });
      if (it == tables.end())
        return false;
      if (toFront) {
        if (std::next(it) != tables.end()) {
          auto moved = std::move(*it);
          tables.erase(it);
          tables.push_back(std::move(moved));
        }
      } else {
        if (it != tables.begin()) {
          auto moved = std::move(*it);
          tables.erase(it);
          tables.insert(tables.begin(), std::move(moved));
        }
      }
      return true;
    }
  }
  return false;
}

void LayoutCollection::ReplaceAll(std::vector<LayoutDefinition> updated) {
  if (updated.empty()) {
    layouts = {DefaultLayout()};
    return;
  }
  layouts = std::move(updated);
}

LayoutDefinition LayoutCollection::DefaultLayout() {
  LayoutDefinition layout;
  layout.name = "Layout 1";
  layout.pageSetup.pageSize = print::PageSize::A4;
  layout.pageSetup.landscape = true;
  layout.view2dViews.clear();
  layout.legendViews.clear();
  layout.eventTables.clear();
  layout.textViews.clear();
  return layout;
}

bool LayoutCollection::NameExists(const std::string &name,
                                  const std::string &ignoreName) const {
  for (const auto &layout : layouts) {
    if (layout.name == name && layout.name != ignoreName)
      return true;
  }
  return false;
}

} // namespace layouts
