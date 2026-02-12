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
#include <wx/choicdlg.h>

LayoutPanel *LayoutPanel::s_instance = nullptr;
wxDEFINE_EVENT(EVT_LAYOUT_SELECTED, wxCommandEvent);

LayoutPanel::LayoutPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  list = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, wxDV_NO_HEADER);
  list->AppendTextColumn("Layout");
  ColumnUtils::EnforceMinColumnWidth(list);

  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(list, 1, wxEXPAND | wxALL, 5);

  wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
  auto *addBtn = new wxButton(this, wxID_ADD, "Add");
  auto *renameBtn = new wxButton(this, wxID_EDIT, "Rename");
  auto *delBtn = new wxButton(this, wxID_DELETE, "Delete");
  btnSizer->Add(addBtn, 0, wxALL, 5);
  btnSizer->Add(renameBtn, 0, wxALL, 5);
  btnSizer->Add(delBtn, 0, wxALL, 5);
  sizer->Add(btnSizer, 0, wxALIGN_LEFT);

  SetSizer(sizer);

  list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &LayoutPanel::OnSelect, this);
  list->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &LayoutPanel::OnContextMenu,
             this);
  list->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &LayoutPanel::OnRenameLayout, this);
  addBtn->Bind(wxEVT_BUTTON, &LayoutPanel::OnAddLayout, this);
  renameBtn->Bind(wxEVT_BUTTON, &LayoutPanel::OnRenameLayout, this);
  delBtn->Bind(wxEVT_BUTTON, &LayoutPanel::OnDeleteLayout, this);

  ReloadLayouts();
}

LayoutPanel *LayoutPanel::Instance() { return s_instance; }

void LayoutPanel::SetInstance(LayoutPanel *p) { s_instance = p; }

void LayoutPanel::ReloadLayouts() {
  if (!list)
    return;

  list->DeleteAllItems();

  const auto &layouts = layouts::LayoutManager::Get().GetLayouts().Items();
  int selectedRow = -1;
  int row = 0;
  for (const auto &layout : layouts) {
    wxVector<wxVariant> cols;
    cols.push_back(wxVariant(wxString::FromUTF8(layout.name)));
    list->AppendItem(cols);
    if (!currentLayout.empty() && layout.name == currentLayout)
      selectedRow = row;
    ++row;
  }

  if (selectedRow < 0 && list->GetItemCount() > 0)
    selectedRow = 0;

  if (selectedRow >= 0) {
    list->SelectRow(selectedRow);
    wxString name = list->GetTextValue(selectedRow, 0);
    currentLayout = name.ToStdString();
    for (const auto &layout : layouts) {
      if (layout.name == currentLayout) {
        EmitLayoutSelected(layout.name);
        break;
      }
    }
  }
}

void LayoutPanel::OnSelect(wxDataViewEvent &evt) {
  unsigned int idx = list->ItemToRow(evt.GetItem());
  if (idx == wxNOT_FOUND)
    return;
  wxString name = list->GetTextValue(idx, 0);
  currentLayout = name.ToStdString();
  const auto &layouts = layouts::LayoutManager::Get().GetLayouts().Items();
  for (const auto &layout : layouts) {
    if (layout.name == currentLayout) {
      EmitLayoutSelected(layout.name);
      break;
    }
  }
}

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

void LayoutPanel::OnRenameLayout(wxCommandEvent &) {
  if (!list)
    return;
  int sel = list->GetSelectedRow();
  if (sel == wxNOT_FOUND)
    return;

  wxString currentName = list->GetTextValue(sel, 0);
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

void LayoutPanel::OnDeleteLayout(wxCommandEvent &) {
  if (!list)
    return;
  int sel = list->GetSelectedRow();
  if (sel == wxNOT_FOUND)
    return;

  if (layouts::LayoutManager::Get().GetLayouts().Count() <= 1) {
    wxMessageBox("Cannot delete the last layout.", "Delete Layout",
                 wxOK | wxICON_ERROR, this);
    return;
  }

  wxString name = list->GetTextValue(sel, 0);
  std::string layoutName = name.ToStdString();
  if (!layouts::LayoutManager::Get().RemoveLayout(layoutName)) {
    wxMessageBox("Could not delete layout.", "Delete Layout",
                 wxOK | wxICON_ERROR, this);
    return;
  }

  if (layoutName == currentLayout)
    currentLayout.clear();
  ReloadLayouts();
}

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
