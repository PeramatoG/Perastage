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

#include <vector>
#include <wx/grid.h>
#include <wx/wx.h>

#include "LayoutCollection.h"
#include "layoutlegenditems.h"

class LayoutLegendEditDialog : public wxDialog {
public:
  LayoutLegendEditDialog(
      wxWindow *parent, const layouts::LayoutLegendDefinition &legend,
      const std::vector<SharedLayoutLegendItem> &availableItems);

  bool GetShowChannelColumn() const;
  std::vector<layouts::LayoutLegendDefinition::ItemSettings> GetItemSettings()
      const;

private:
  struct RowData {
    std::string typeName;
    bool visible = true;
    bool showBottomSymbol = true;
    bool showFrontSymbol = true;
    bool showSideSymbol = false;
    std::string customName;
  };

  void PopulateGrid();
  void MoveSelectedRow(int delta);
  void SaveFromGrid();

  std::vector<RowData> rows_;
  wxGrid *grid_ = nullptr;
  wxCheckBox *showChannelColumnCheck_ = nullptr;
};
