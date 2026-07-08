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

// Identifies supported reusable GDTF type identity fields.
enum class GdtfTypeIdentityField {
  FixtureTypeName,
  Manufacturer,
  ModelName,
  SourceFileReference
};

struct GdtfTypeIdentityPresentation {
  GdtfTypeIdentityField field;
  std::string label;
  std::string value;
  bool visible = true;
  bool editable = true;
  bool showActionButton = false;
  std::string actionLabel;
};

class GdtfTypeIdentityPanel : public wxPanel {
public:
  using ChangeCallback =
      std::function<void(GdtfTypeIdentityField field, const std::string &value)>;
  using ActionCallback = std::function<void(GdtfTypeIdentityField field)>;

  explicit GdtfTypeIdentityPanel(wxWindow *parent);

  void ConfigureFields(const std::vector<GdtfTypeIdentityPresentation> &fields);
  void SetFieldValue(GdtfTypeIdentityField field, const std::string &value);
  std::optional<std::string> GetFieldValue(GdtfTypeIdentityField field) const;
  std::map<GdtfTypeIdentityField, std::string> GetValues() const;
  void SetFieldEditable(GdtfTypeIdentityField field, bool editable);
  void SetChangeCallback(ChangeCallback callback);
  void SetActionCallback(ActionCallback callback);

private:
  struct FieldControls {
    wxStaticText *label = nullptr;
    wxTextCtrl *value = nullptr;
  };

  void ClearFields();
  void NotifyFieldChanged(GdtfTypeIdentityField field);

  wxFlexGridSizer *grid = nullptr;
  std::map<GdtfTypeIdentityField, FieldControls> controls;
  ChangeCallback changeCallback;
  ActionCallback actionCallback;
  bool updating = false;
};
