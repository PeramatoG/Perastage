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

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <wx/panel.h>

class wxFlexGridSizer;
class wxStaticText;
class wxTextCtrl;

// Identifies supported reusable GDTF physical/type property fields.
enum class GdtfPhysicalPropertyField {
  PowerConsumption,
  Weight,
  Length,
  Width,
  Height,
  CrossSection
};

struct GdtfPhysicalPropertyPresentation {
  GdtfPhysicalPropertyField field;
  std::string label;
  std::string value;
  bool visible = true;
  bool editable = true;
  std::string unitSuffix;
  std::string helpText;
};

class GdtfPhysicalPropertiesPanel : public wxPanel {
public:
  using ChangeCallback =
      std::function<void(GdtfPhysicalPropertyField field, const std::string &value)>;

  explicit GdtfPhysicalPropertiesPanel(wxWindow *parent);

  void ConfigureFields(const std::vector<GdtfPhysicalPropertyPresentation> &fields);
  void SetFieldValue(GdtfPhysicalPropertyField field, const std::string &value);
  std::optional<std::string> GetFieldValue(GdtfPhysicalPropertyField field) const;
  std::map<GdtfPhysicalPropertyField, std::string> GetValues() const;
  void SetFieldEditable(GdtfPhysicalPropertyField field, bool editable);
  void SetFieldValidation(GdtfPhysicalPropertyField field, const std::string &message);
  void SetChangeCallback(ChangeCallback callback);

private:
  struct FieldControls {
    wxStaticText *label = nullptr;
    wxTextCtrl *value = nullptr;
    wxStaticText *suffix = nullptr;
    wxStaticText *validation = nullptr;
  };

  void ClearFields();
  void NotifyFieldChanged(GdtfPhysicalPropertyField field);

  wxFlexGridSizer *grid = nullptr;
  std::map<GdtfPhysicalPropertyField, FieldControls> controls;
  ChangeCallback changeCallback;
  bool updating = false;
};
