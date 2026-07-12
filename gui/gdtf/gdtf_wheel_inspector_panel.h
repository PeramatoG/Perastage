/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <string>
#include <vector>

#include <wx/panel.h>

class wxListBox;
class wxTextCtrl;

struct GdtfWheelInspectorSlotPresentation {
  std::string label;
  bool selected = false;
};

struct GdtfWheelInspectorPresentation {
  std::string activeText;
  std::vector<GdtfWheelInspectorSlotPresentation> slots;
};

class GdtfWheelInspectorPanel : public wxPanel {
public:
  explicit GdtfWheelInspectorPanel(wxWindow *parent);

  void SetPresentation(const GdtfWheelInspectorPresentation &presentation);
  void ClearPresentation();

private:
  wxTextCtrl *activeTextCtrl = nullptr;
  wxListBox *slotList = nullptr;
};
