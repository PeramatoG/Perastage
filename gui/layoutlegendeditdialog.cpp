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
#include "layoutlegendeditdialog.h"

#include <unordered_map>

namespace {
enum GridColumn {
  kColType = 0,
  kColVisible,
  kColBottom,
  kColFront,
  kColSide,
  kColCustomName,
  kColCount,
};

wxString BoolCellValue(bool value) {
  // wxGrid bool editor expects "1" for true and empty string for false.
  // Using "0" can trigger assertions in debug wxWidgets builds.
  return value ? "1" : "";
}
}

LayoutLegendEditDialog::LayoutLegendEditDialog(
    wxWindow *parent, const layouts::LayoutLegendDefinition &legend,
    const std::vector<SharedLayoutLegendItem> &availableItems)
    : wxDialog(parent, wxID_ANY, "Edit Legend", wxDefaultPosition,
               wxSize(860, 520), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  std::unordered_map<std::string, layouts::LayoutLegendDefinition::ItemSettings>
      existingByType;
  existingByType.reserve(legend.itemSettings.size());
  for (const auto &item : legend.itemSettings) {
    existingByType[item.typeName] = item;
  }

  rows_.reserve(availableItems.size());
  for (const auto &item : availableItems) {
    RowData row;
    row.typeName = item.typeName;
    if (const auto it = existingByType.find(item.typeName);
        it != existingByType.end()) {
      row.visible = it->second.visible;
      row.showBottomSymbol = it->second.showBottomSymbol;
      row.showFrontSymbol = it->second.showFrontSymbol;
      row.showSideSymbol = it->second.showSideSymbol;
      row.customName = it->second.customName;
    }
    rows_.push_back(std::move(row));
  }

  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

  showChannelColumnCheck_ =
      new wxCheckBox(this, wxID_ANY, _("Show channel count column"));
  showChannelColumnCheck_->SetValue(legend.showChannelColumn);
  mainSizer->Add(showChannelColumnCheck_, 0, wxALL, 10);

  grid_ = new wxGrid(this, wxID_ANY);
  grid_->CreateGrid(0, kColCount);
  grid_->EnableEditing(true);
  grid_->SetColLabelValue(kColType, "Type");
  grid_->SetColLabelValue(kColVisible, "Visible");
  grid_->SetColLabelValue(kColBottom, "Bottom");
  grid_->SetColLabelValue(kColFront, "Front");
  grid_->SetColLabelValue(kColSide, "Side");
  grid_->SetColLabelValue(kColCustomName, "Custom name");
  grid_->SetColFormatBool(kColVisible);
  grid_->SetColFormatBool(kColBottom);
  grid_->SetColFormatBool(kColFront);
  grid_->SetColFormatBool(kColSide);
  grid_->SetColSize(kColType, 240);
  grid_->SetColSize(kColVisible, 80);
  grid_->SetColSize(kColBottom, 80);
  grid_->SetColSize(kColFront, 80);
  grid_->SetColSize(kColSide, 80);
  grid_->SetColSize(kColCustomName, 220);
  PopulateGrid();

  mainSizer->Add(grid_, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

  wxBoxSizer *moveSizer = new wxBoxSizer(wxHORIZONTAL);
  wxButton *upButton = new wxButton(this, wxID_ANY, _("Move Up"));
  wxButton *downButton = new wxButton(this, wxID_ANY, _("Move Down"));
  moveSizer->Add(upButton, 0, wxRIGHT, 8);
  moveSizer->Add(downButton, 0);
  mainSizer->Add(moveSizer, 0, wxLEFT | wxRIGHT | wxTOP, 10);

  mainSizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0,
                 wxEXPAND | wxALL, 10);

  upButton->Bind(wxEVT_BUTTON,
                 [this](wxCommandEvent &) { MoveSelectedRow(-1); });
  downButton->Bind(wxEVT_BUTTON,
                   [this](wxCommandEvent &) { MoveSelectedRow(1); });
  Bind(wxEVT_BUTTON,
       [this](wxCommandEvent &event) {
         if (event.GetId() == wxID_OK)
           SaveFromGrid();
         event.Skip();
       });

  SetSizerAndFit(mainSizer);
  CentreOnParent();
}

bool LayoutLegendEditDialog::GetShowChannelColumn() const {
  return showChannelColumnCheck_ ? showChannelColumnCheck_->GetValue() : true;
}

std::vector<layouts::LayoutLegendDefinition::ItemSettings>
LayoutLegendEditDialog::GetItemSettings() const {
  std::vector<layouts::LayoutLegendDefinition::ItemSettings> settings;
  settings.reserve(rows_.size());
  for (const auto &row : rows_) {
    layouts::LayoutLegendDefinition::ItemSettings item;
    item.typeName = row.typeName;
    item.visible = row.visible;
    item.showBottomSymbol = row.showBottomSymbol;
    item.showFrontSymbol = row.showFrontSymbol;
    item.showSideSymbol = row.showSideSymbol;
    item.customName = row.customName;
    settings.push_back(std::move(item));
  }
  return settings;
}

void LayoutLegendEditDialog::PopulateGrid() {
  if (!grid_)
    return;
  const int currentRows = grid_->GetNumberRows();
  if (currentRows > 0)
    grid_->DeleteRows(0, currentRows);
  if (!rows_.empty())
    grid_->AppendRows(static_cast<int>(rows_.size()));

  for (int row = 0; row < static_cast<int>(rows_.size()); ++row) {
    const RowData &data = rows_[static_cast<size_t>(row)];
    grid_->SetCellValue(row, kColType, wxString::FromUTF8(data.typeName));
    grid_->SetReadOnly(row, kColType, true);
    grid_->SetCellValue(row, kColVisible, BoolCellValue(data.visible));
    grid_->SetCellValue(row, kColBottom, BoolCellValue(data.showBottomSymbol));
    grid_->SetCellValue(row, kColFront, BoolCellValue(data.showFrontSymbol));
    grid_->SetCellValue(row, kColSide, BoolCellValue(data.showSideSymbol));
    grid_->SetCellValue(row, kColCustomName, wxString::FromUTF8(data.customName));
  }
}

void LayoutLegendEditDialog::MoveSelectedRow(int delta) {
  if (!grid_)
    return;
  SaveFromGrid();
  const int row = grid_->GetGridCursorRow();
  if (row < 0 || row >= static_cast<int>(rows_.size()))
    return;
  const int target = row + delta;
  if (target < 0 || target >= static_cast<int>(rows_.size()))
    return;
  std::swap(rows_[static_cast<size_t>(row)], rows_[static_cast<size_t>(target)]);
  PopulateGrid();
  grid_->SetGridCursor(target, grid_->GetGridCursorCol());
  grid_->MakeCellVisible(target, grid_->GetGridCursorCol());
}

void LayoutLegendEditDialog::SaveFromGrid() {
  if (!grid_)
    return;
  if (grid_->IsCellEditControlShown())
    grid_->HideCellEditControl();
  grid_->SaveEditControlValue();

  for (int row = 0; row < static_cast<int>(rows_.size()); ++row) {
    RowData &data = rows_[static_cast<size_t>(row)];
    data.visible = !grid_->GetCellValue(row, kColVisible).IsEmpty();
    data.showBottomSymbol = !grid_->GetCellValue(row, kColBottom).IsEmpty();
    data.showFrontSymbol = !grid_->GetCellValue(row, kColFront).IsEmpty();
    data.showSideSymbol = !grid_->GetCellValue(row, kColSide).IsEmpty();
    data.customName = std::string(grid_->GetCellValue(row, kColCustomName).utf8_string());
  }
}
