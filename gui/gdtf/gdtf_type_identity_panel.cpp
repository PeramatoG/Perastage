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
#include "gdtf_type_identity_panel.h"

#include <utility>

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

// Creates the type identity panel layout.
GdtfTypeIdentityPanel::GdtfTypeIdentityPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  grid = new wxFlexGridSizer(2, 5, 5);
  grid->AddGrowableCol(1, 1);
  auto *root = new wxBoxSizer(wxVERTICAL);
  root->Add(grid, 0, wxEXPAND);
  SetSizer(root);
}

// Configures the visible identity controls.
void GdtfTypeIdentityPanel::ConfigureFields(
    const std::vector<GdtfTypeIdentityPresentation> &fields) {
  updating = true;
  ClearFields();
  for (const auto &field : fields) {
    if (!field.visible)
      continue;
    FieldControls row;
    row.label = new wxStaticText(this, wxID_ANY, wxString::FromUTF8(field.label));
    row.value = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(field.value));
    row.value->Enable(field.editable);
    row.value->Bind(wxEVT_TEXT, [this, id = field.field](wxCommandEvent &) {
      NotifyFieldChanged(id);
    });
    grid->Add(row.label, 0, wxALIGN_CENTER_VERTICAL);
    if (field.showActionButton) {
      auto *line = new wxBoxSizer(wxHORIZONTAL);
      line->Add(row.value, 1, wxEXPAND | wxRIGHT, 5);
      auto *button = new wxButton(this, wxID_ANY,
                                  field.actionLabel.empty()
                                      ? wxString(_("..."))
                                      : wxString::FromUTF8(field.actionLabel));
      button->Bind(wxEVT_BUTTON, [this, id = field.field](wxCommandEvent &) {
        if (actionCallback)
          actionCallback(id);
      });
      line->Add(button, 0);
      grid->Add(line, 1, wxEXPAND);
    } else {
      grid->Add(row.value, 1, wxEXPAND);
    }
    controls.emplace(field.field, row);
  }
  updating = false;
  Layout();
}

// Sets one identity value without notifying the host.
void GdtfTypeIdentityPanel::SetFieldValue(GdtfTypeIdentityField field,
                                          const std::string &value) {
  auto it = controls.find(field);
  if (it == controls.end() || !it->second.value)
    return;
  updating = true;
  it->second.value->SetValue(wxString::FromUTF8(value));
  updating = false;
}

// Returns one current identity value when visible.
std::optional<std::string> GdtfTypeIdentityPanel::GetFieldValue(
    GdtfTypeIdentityField field) const {
  auto it = controls.find(field);
  if (it == controls.end() || !it->second.value)
    return std::nullopt;
  return std::string(it->second.value->GetValue().ToUTF8());
}

// Returns all currently visible identity values.
std::map<GdtfTypeIdentityField, std::string> GdtfTypeIdentityPanel::GetValues()
    const {
  std::map<GdtfTypeIdentityField, std::string> values;
  for (const auto &[field, row] : controls) {
    if (row.value)
      values[field] = std::string(row.value->GetValue().ToUTF8());
  }
  return values;
}

// Updates editability for one visible field.
void GdtfTypeIdentityPanel::SetFieldEditable(GdtfTypeIdentityField field,
                                             bool editable) {
  auto it = controls.find(field);
  if (it != controls.end() && it->second.value)
    it->second.value->Enable(editable);
}

// Displays validation feedback supplied by the host as a tooltip.
void GdtfTypeIdentityPanel::SetFieldValidation(GdtfTypeIdentityField field,
                                               const std::string &message) {
  auto it = controls.find(field);
  if (it != controls.end() && it->second.value)
    it->second.value->SetToolTip(wxString::FromUTF8(message));
}

// Registers the host field-change callback.
void GdtfTypeIdentityPanel::SetChangeCallback(ChangeCallback callback) {
  changeCallback = std::move(callback);
}

// Registers the host action callback.
void GdtfTypeIdentityPanel::SetActionCallback(ActionCallback callback) {
  actionCallback = std::move(callback);
}

// Removes existing controls before rebuilding the field list.
void GdtfTypeIdentityPanel::ClearFields() {
  controls.clear();
  grid->Clear(true);
}

// Notifies the host about a genuine user edit.
void GdtfTypeIdentityPanel::NotifyFieldChanged(GdtfTypeIdentityField field) {
  if (updating || !changeCallback)
    return;
  auto value = GetFieldValue(field);
  changeCallback(field, value.value_or(std::string()));
}
