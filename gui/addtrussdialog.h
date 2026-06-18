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

#include <array>

#include <wx/dialog.h>

class wxCheckBox;
class wxCommandEvent;
class wxSpinCtrl;
class wxSpinCtrlDouble;

struct AddTrussRequest {
  int quantity = 1;
  std::array<float, 3> insertionPointMm{0.0f, 0.0f, 0.0f};
  bool createGroup = true;
  bool continuousPlacement = false;
};

class AddTrussDialog final : public wxDialog {
public:
  explicit AddTrussDialog(wxWindow *parent);

  AddTrussRequest GetRequest() const;

private:
  void OnContinuousPlacementChanged(wxCommandEvent &event);

  wxSpinCtrl *quantityCtrl_ = nullptr;
  wxSpinCtrlDouble *xCtrl_ = nullptr;
  wxSpinCtrlDouble *yCtrl_ = nullptr;
  wxSpinCtrlDouble *zCtrl_ = nullptr;
  wxCheckBox *createGroupCtrl_ = nullptr;
  wxCheckBox *continuousPlacementCtrl_ = nullptr;
};
