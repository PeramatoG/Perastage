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

#include "gdtf_metadata_summary.h"

#include <array>
#include <cstddef>
#include <string>

#include <wx/panel.h>
#include <wx/string.h>

class wxStaticText;
class wxTextCtrl;

class GdtfMetadataPanel : public wxPanel {
public:
  explicit GdtfMetadataPanel(wxWindow *parent);

  void SetMetadata(const GdtfMetadataSummary &summary);
  void SetUnavailable();

private:
  wxString ValueOrFallback(const std::string &value) const;
  void SetValues(const std::array<wxString, 8> &values);
  void RewrapValueLabels(bool force = false);
  int WrapWidth() const;

  wxTextCtrl *descriptionCtrl = nullptr;
  std::array<wxStaticText *, 8> valueLabels{};
  std::array<wxString, 8> currentValues{};
  int lastAppliedWrapWidth = -1;
};
