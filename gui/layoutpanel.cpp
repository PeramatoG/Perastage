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
#include "layoutpanel.h"

#include "columnutils.h"
#include "LayoutManager.h"
#include "LayoutSelectionPolicy.h"
#include <wx/choicdlg.h>
#include <wx/filedlg.h>

#include <vector>

LayoutPanel *LayoutPanel::s_instance = nullptr;
wxDEFINE_EVENT(EVT_LAYOUT_SELECTED, wxCommandEvent);

// Builds the layout management panel and wires its actions.
LayoutPanel::LayoutPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  list = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, wxDV_NO_HEADER);
  list->AppendTextColumn("Layout");
  ColumnUtils::EnforceMinColumnWidth(list);

  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(list, 1, wxEXPAND | wxALL, 5);

  wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
  auto *addBtn = new wxButton(this, wxID_ADD, "Add");
  renameButton = new wxButton(this, wxID_EDIT, "Rename");
  deleteButton = new wxButton(this, wxID_DELETE, "Delete");
  exportTemplateButton =
      new wxButton(this, wxID_ANY, "Export template");
  auto *importTemplateBtn =
      new wxButton(this, wxID_ANY, "Import template");
  btnSizer->Add(addBtn, 0, wxALL, 5);
  btnSizer->Add(renameButton, 0, wxALL, 5);
  btnSizer->Add(deleteButton, 0, wxALL, 5);
  btnSizer->Add(exportTemplateButton, 0, wxALL, 5);
  btnSizer->Add(importTemplateBtn, 0, wxALL, 5);
  sizer->Add(btnSizer, 0, wxALIGN_LEFT);

  SetSizer(sizer);

  list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &LayoutPanel::OnSelect, this);
  list->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &LayoutPanel::OnContextMenu,
             this);
  list->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &LayoutPanel::OnRenameLayout, this);
  addBtn->Bind(wxEVT_BUTTON, &LayoutPanel::OnAddLayout, this);
  renameButton->Bind(wxEVT_BUTTON, &LayoutPanel::OnRenameLayout, this);
  deleteButton->Bind(wxEVT_BUTTON, &LayoutPanel::OnDeleteLayout, this);
  exportTemplateButton->Bind(wxEVT_BUTTON,
                             &LayoutPanel::OnExportLayoutTemplate, this);
  importTemplateBtn->Bind(wxEVT_BUTTON,
                          &LayoutPanel::OnImportLayoutTemplate, this);

  ReloadLayouts();
}

// Returns the process-wide layout panel instance when available.
LayoutPanel *LayoutPanel::Instance() { return s_instance; }

// Stores the process-wide layout panel instance pointer.
void LayoutPanel::SetInstance(LayoutPanel *p) { s_instance = p; }

// Sets the layout row that should remain selected during the next reload.
void LayoutPanel::SetCurrentLayout(const std::string &layoutName) {
  currentLayout = layoutName;
}

// Reloads available project layouts and repairs the active selection.
void LayoutPanel::ReloadLayouts() {
  if (!list)
    return;

  repairingSelection = true;
  list->DeleteAllItems();

  const auto &layouts = layouts::LayoutManager::Get().GetLayouts().Items();
  for (const auto &layout : layouts) {
    wxVector<wxVariant> cols;
    cols.push_back(wxVariant(wxString::FromUTF8(layout.name)));
    list->AppendItem(cols);
  }
  repairingSelection = false;

  const int selectedRow = EnsureValidSelection(false);
  if (selectedRow != wxNOT_FOUND && !currentLayout.empty())
    EmitLayoutSelected(currentLayout);
  UpdateActionAvailability();
}

// Finds the list row for a layout name or reports that it is absent.
int LayoutPanel::FindLayoutRow(const std::string &layoutName) const {
  if (!list || layoutName.empty())
    return wxNOT_FOUND;
  for (unsigned int row = 0; row < list->GetItemCount(); ++row) {
    if (list->GetTextValue(row, 0).ToStdString() == layoutName)
      return static_cast<int>(row);
  }
  return wxNOT_FOUND;
}

// Restores a valid layout selection when layouts exist.
int LayoutPanel::EnsureValidSelection(bool emitSelectionEvent, int preferredRow) {
  if (!list || list->GetItemCount() == 0) {
    currentLayout.clear();
    UpdateActionAvailability();
    return wxNOT_FOUND;
  }

  std::vector<std::string> names;
  names.reserve(list->GetItemCount());
  for (unsigned int row = 0; row < list->GetItemCount(); ++row)
    names.push_back(list->GetTextValue(row, 0).ToStdString());

  int selectedRow = list->GetSelectedRow();
  if (selectedRow == wxNOT_FOUND) {
    const auto chosen = layouts::ChooseLayoutSelectionRow(
        layouts::LayoutSelectionRequest{names, currentLayout, preferredRow});
    selectedRow = chosen.has_value() ? *chosen : wxNOT_FOUND;
  }
  if (selectedRow == wxNOT_FOUND) {
    UpdateActionAvailability();
    return wxNOT_FOUND;
  }

  const std::string selectedName = names[static_cast<size_t>(selectedRow)];
  const bool changed = selectedName != currentLayout;
  repairingSelection = true;
  list->SelectRow(selectedRow);
  list->SetCurrentItem(list->RowToItem(selectedRow));
  repairingSelection = false;
  currentLayout = selectedName;
  UpdateActionAvailability();
  if (changed && emitSelectionEvent)
    EmitLayoutSelected(currentLayout);
  return selectedRow;
}

// Returns the selected layout name after repairing accidental selection loss.
std::optional<std::string> LayoutPanel::GetSelectedLayoutNameOrRestore() {
  const int row = EnsureValidSelection(false);
  if (row == wxNOT_FOUND)
    return std::nullopt;
  return list->GetTextValue(row, 0).ToStdString();
}

// Updates layout action buttons based on available layouts.
void LayoutPanel::UpdateActionAvailability() {
  const auto count = layouts::LayoutManager::Get().GetLayouts().Count();
  if (renameButton)
    renameButton->Enable(count > 0);
  if (exportTemplateButton)
    exportTemplateButton->Enable(count > 0);
  if (deleteButton)
    deleteButton->Enable(count > 1);
}

// Handles user layout selection changes and repairs accidental selection loss.
void LayoutPanel::OnSelect(wxDataViewEvent &evt) {
  if (repairingSelection)
    return;
  unsigned int idx = list->ItemToRow(evt.GetItem());
  if (idx == wxNOT_FOUND) {
    CallAfter([this]() { EnsureValidSelection(true); });
    return;
  }
  wxString name = list->GetTextValue(idx, 0);
  const std::string selectedLayout = name.ToStdString();
  if (selectedLayout == currentLayout)
    return;

  currentLayout = selectedLayout;
  UpdateActionAvailability();
  const auto &layouts = layouts::LayoutManager::Get().GetLayouts().Items();
  for (const auto &layout : layouts) {
    if (layout.name == currentLayout) {
      EmitLayoutSelected(layout.name);
      break;
    }
  }
}

// Opens the layout context menu for the selected row.
void LayoutPanel::OnContextMenu(wxDataViewEvent &evt) {
  unsigned int idx = list->ItemToRow(evt.GetItem());
  if (idx == wxNOT_FOUND)
    return;

  list->SelectRow(idx);

  wxString name = list->GetTextValue(idx, 0);
  std::string layoutName = name.ToStdString();
  const auto &layouts = layouts::LayoutManager::Get().GetLayouts().Items();
  const layouts::LayoutDefinition *target = nullptr;
  for (const auto &layout : layouts) {
    if (layout.name == layoutName) {
      target = &layout;
      break;
    }
  }
  if (!target)
    return;

  wxMenu menu;
  auto *orientationMenu = new wxMenu();
  auto *portraitItem = orientationMenu->AppendRadioItem(wxID_ANY, "Vertical");
  auto *landscapeItem =
      orientationMenu->AppendRadioItem(wxID_ANY, "Horizontal");
  if (target->pageSetup.landscape)
    landscapeItem->Check(true);
  else
    portraitItem->Check(true);
  menu.AppendSubMenu(orientationMenu, "Orientation");

  int portraitId = portraitItem->GetId();
  int landscapeId = landscapeItem->GetId();

  menu.Bind(
      wxEVT_MENU,
      [this, layoutName](wxCommandEvent &) {
        if (layouts::LayoutManager::Get().SetLayoutOrientation(layoutName,
                                                               false)) {
          EmitLayoutSelected(layoutName);
        }
      },
      portraitId);
  menu.Bind(
      wxEVT_MENU,
      [this, layoutName](wxCommandEvent &) {
        if (layouts::LayoutManager::Get().SetLayoutOrientation(layoutName,
                                                               true)) {
          EmitLayoutSelected(layoutName);
        }
      },
      landscapeId);

  PopupMenu(&menu);
}

// Adds a new empty layout with a user-provided name.
void LayoutPanel::OnAddLayout(wxCommandEvent &) {
  wxTextEntryDialog nameDlg(this, "Enter new layout name:", "Add Layout");
  if (nameDlg.ShowModal() != wxID_OK)
    return;
  std::string name = nameDlg.GetValue().ToStdString();
  if (name.empty())
    return;

  const auto &items = layouts::LayoutManager::Get().GetLayouts().Items();
  for (const auto &layout : items) {
    if (layout.name == name) {
      wxMessageBox("Layout already exists.", "Add Layout",
                   wxOK | wxICON_ERROR, this);
      return;
    }
  }

  layouts::LayoutDefinition layout;
  layout.name = name;
  layout.pageSetup.pageSize = print::PageSize::A4;
  layout.pageSetup.landscape = true;

  if (!layouts::LayoutManager::Get().AddLayout(layout)) {
    wxMessageBox("Could not add layout.", "Add Layout", wxOK | wxICON_ERROR,
                 this);
    return;
  }

  currentLayout = name;
  ReloadLayouts();
}

// Renames the currently selected layout after repairing selection state.
void LayoutPanel::OnRenameLayout(wxCommandEvent &) {
  const auto selectedLayout = GetSelectedLayoutNameOrRestore();
  if (!selectedLayout) {
    wxMessageBox("No layout is available.", "Rename Layout",
                 wxOK | wxICON_ERROR, this);
    return;
  }

  wxString currentName = wxString::FromUTF8(*selectedLayout);
  wxTextEntryDialog dlg(this, "Enter new layout name:", "Rename Layout",
                        currentName);
  if (dlg.ShowModal() != wxID_OK)
    return;

  std::string newName = dlg.GetValue().ToStdString();
  std::string oldName = currentName.ToStdString();
  if (newName.empty() || newName == oldName)
    return;

  if (!layouts::LayoutManager::Get().RenameLayout(oldName, newName)) {
    wxMessageBox("Layout name is not available.", "Rename Layout",
                 wxOK | wxICON_ERROR, this);
    return;
  }

  currentLayout = newName;
  ReloadLayouts();
}

// Deletes the selected layout while preserving a nearby active selection.
void LayoutPanel::OnDeleteLayout(wxCommandEvent &) {
  const auto selectedLayout = GetSelectedLayoutNameOrRestore();
  if (!selectedLayout) {
    wxMessageBox("No layout is available.", "Delete Layout",
                 wxOK | wxICON_ERROR, this);
    return;
  }

  if (layouts::LayoutManager::Get().GetLayouts().Count() <= 1) {
    wxMessageBox("Cannot delete the last layout.", "Delete Layout",
                 wxOK | wxICON_ERROR, this);
    return;
  }

  std::string layoutName = *selectedLayout;
  if (!layouts::LayoutManager::Get().RemoveLayout(layoutName)) {
    wxMessageBox("Could not delete layout.", "Delete Layout",
                 wxOK | wxICON_ERROR, this);
    return;
  }

  const int fallbackRow = FindLayoutRow(layoutName);
  if (layoutName == currentLayout) {
    std::vector<std::string> remainingNames;
    for (const auto &layout : layouts::LayoutManager::Get().GetLayouts().Items())
      remainingNames.push_back(layout.name);
    const auto chosen = layouts::ChooseLayoutSelectionRow(
        layouts::LayoutSelectionRequest{remainingNames, {}, fallbackRow});
    currentLayout = chosen.has_value() ? remainingNames[static_cast<size_t>(*chosen)]
                                       : std::string();
  }
  ReloadLayouts();
}

// Exports the selected layout as a portable package.
void LayoutPanel::OnExportLayoutTemplate(wxCommandEvent &) {
  const auto selectedLayout = GetSelectedLayoutNameOrRestore();
  if (!selectedLayout) {
    wxMessageBox("No layout is available to export.", "Export layout package",
                 wxOK | wxICON_ERROR, this);
    return;
  }

  const wxString selectedName = wxString::FromUTF8(*selectedLayout);
  wxFileDialog saveDialog(this, "Export layout package", wxEmptyString,
                          selectedName + ".pslayout",
                          "Perastage layout packages (*.pslayout)|*.pslayout",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDialog.ShowModal() != wxID_OK)
    return;

  std::string error;
  if (!layouts::LayoutManager::Get().ExportLayoutTemplate(
          selectedName.ToStdString(), saveDialog.GetPath().ToStdString(),
          &error)) {
    wxMessageBox("Could not export layout package.\n" +
                     wxString::FromUTF8(error),
                 "Export layout package", wxOK | wxICON_ERROR, this);
    return;
  }

  wxMessageBox("Layout package exported successfully.",
               "Export layout package", wxOK | wxICON_INFORMATION, this);
}

// Imports a portable or legacy layout template file.
void LayoutPanel::OnImportLayoutTemplate(wxCommandEvent &) {
  wxFileDialog openDialog(this, "Import layout template", wxEmptyString,
                          wxEmptyString,
                          "All supported layout templates (*.pslayout;*.json)|*.pslayout;*.json|Perastage layout packages (*.pslayout)|*.pslayout|Legacy JSON layout templates (*.json)|*.json",
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (openDialog.ShowModal() != wxID_OK)
    return;

  std::string importedLayoutName;
  std::string error;
  if (!layouts::LayoutManager::Get().ImportLayoutTemplate(
          openDialog.GetPath().ToStdString(), std::string(),
          &importedLayoutName, &error)) {
    wxMessageBox("Could not import layout template.\n" +
                     wxString::FromUTF8(error),
                 "Import template", wxOK | wxICON_ERROR, this);
    return;
  }

  currentLayout = importedLayoutName;
  ReloadLayouts();
}

// Posts the active layout selection event to listeners.
void LayoutPanel::EmitLayoutSelected(const std::string &layoutName) {
  if (layoutName.empty())
    return;
  wxCommandEvent event(EVT_LAYOUT_SELECTED);
  event.SetEventObject(this);
  event.SetString(wxString::FromUTF8(layoutName));
  if (GetParent())
    wxPostEvent(GetParent(), event);
  else
    wxPostEvent(this, event);
}
