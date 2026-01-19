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
#pragma once

#include "LayoutCollection.h"

class ConfigManager;

namespace layouts {

class LayoutManager {
public:
  static LayoutManager &Get();

  const LayoutCollection &GetLayouts() const;

  bool AddLayout(const LayoutDefinition &layout);
  bool RenameLayout(const std::string &currentName,
                    const std::string &newName);
  bool RemoveLayout(const std::string &name);
  bool SetLayoutOrientation(const std::string &name, bool landscape);
  bool UpdateLayout2DView(const std::string &name,
                          const Layout2DViewDefinition &view);
  bool RemoveLayout2DView(const std::string &name, int viewId);
  bool MoveLayout2DView(const std::string &name, int viewId, bool toFront);
  bool UpdateLayoutLegend(const std::string &name,
                          const LayoutLegendDefinition &legend);
  bool RemoveLayoutLegend(const std::string &name, int legendId);
  bool MoveLayoutLegend(const std::string &name, int legendId, bool toFront);
  bool UpdateLayoutEventTable(const std::string &name,
                              const LayoutEventTableDefinition &table);
  bool RemoveLayoutEventTable(const std::string &name, int tableId);
  bool MoveLayoutEventTable(const std::string &name, int tableId, bool toFront);
  bool UpdateLayoutText(const std::string &name,
                        const LayoutTextDefinition &text);
  bool RemoveLayoutText(const std::string &name, int textId);
  bool MoveLayoutText(const std::string &name, int textId, bool toFront);
  bool UpdateLayoutImage(const std::string &name,
                         const LayoutImageDefinition &image);
  bool RemoveLayoutImage(const std::string &name, int imageId);
  bool MoveLayoutImage(const std::string &name, int imageId, bool toFront);

  void LoadFromConfig(ConfigManager &cfg);
  void SaveToConfig(ConfigManager &cfg) const;
  void ResetToDefault(ConfigManager &cfg);

private:
  LayoutManager();
  void SyncToConfig();

  LayoutCollection layouts;
};

} // namespace layouts
