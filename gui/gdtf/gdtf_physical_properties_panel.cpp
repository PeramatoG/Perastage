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
#include "gdtf_physical_properties_panel.h"

#include <utility>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

// Creates the physical properties panel layout.
GdtfPhysicalPropertiesPanel::GdtfPhysicalPropertiesPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  grid = new wxFlexGridSizer(3, 5, 5);
  grid->AddGrowableCol(1, 1);
  auto *root = new wxBoxSizer(wxVERTICAL);
  root->Add(grid, 0, wxEXPAND);
  SetSizer(root);
}

// Configures the visible physical property controls.
void GdtfPhysicalPropertiesPanel::ConfigureFields(
    const std::vector<GdtfPhysicalPropertyPresentation> &fields) {
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
    row.suffix = new wxStaticText(this, wxID_ANY, wxString::FromUTF8(field.unitSuffix));
    if (!field.helpText.empty())
      row.value->SetToolTip(wxString::FromUTF8(field.helpText));
    grid->Add(row.label, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(row.value, 1, wxEXPAND);
    grid->Add(row.suffix, 0, wxALIGN_CENTER_VERTICAL);
    controls.emplace(field.field, row);
  }
  updating = false;
  Layout();
}

// Sets one field value without notifying the host.
void GdtfPhysicalPropertiesPanel::SetFieldValue(
    GdtfPhysicalPropertyField field, const std::string &value) {
  auto it = controls.find(field);
  if (it == controls.end() || !it->second.value)
    return;
  updating = true;
  it->second.value->SetValue(wxString::FromUTF8(value));
  updating = false;
}

// Returns one current field value when the field is visible.
std::optional<std::string> GdtfPhysicalPropertiesPanel::GetFieldValue(
    GdtfPhysicalPropertyField field) const {
  auto it = controls.find(field);
  if (it == controls.end() || !it->second.value)
    return std::nullopt;
  return std::string(it->second.value->GetValue().ToUTF8());
}

// Returns all currently visible field values.
std::map<GdtfPhysicalPropertyField, std::string>
GdtfPhysicalPropertiesPanel::GetValues() const {
  std::map<GdtfPhysicalPropertyField, std::string> values;
  for (const auto &[field, row] : controls) {
    if (row.value)
      values[field] = std::string(row.value->GetValue().ToUTF8());
  }
  return values;
}

// Updates editability for one visible field.
void GdtfPhysicalPropertiesPanel::SetFieldEditable(
    GdtfPhysicalPropertyField field, bool editable) {
  auto it = controls.find(field);
  if (it != controls.end() && it->second.value)
    it->second.value->Enable(editable);
}

// Displays validation feedback supplied by the host as a tooltip.
void GdtfPhysicalPropertiesPanel::SetFieldValidation(
    GdtfPhysicalPropertyField field, const std::string &message) {
  auto it = controls.find(field);
  if (it != controls.end() && it->second.value)
    it->second.value->SetToolTip(wxString::FromUTF8(message));
}

// Registers the host field-change callback.
void GdtfPhysicalPropertiesPanel::SetChangeCallback(ChangeCallback callback) {
  changeCallback = std::move(callback);
}

// Removes existing controls before rebuilding the field list.
void GdtfPhysicalPropertiesPanel::ClearFields() {
  controls.clear();
  grid->Clear(true);
}

// Notifies the host about a genuine user edit.
void GdtfPhysicalPropertiesPanel::NotifyFieldChanged(
    GdtfPhysicalPropertyField field) {
  if (updating || !changeCallback)
    return;
  auto value = GetFieldValue(field);
  changeCallback(field, value.value_or(std::string()));
}
