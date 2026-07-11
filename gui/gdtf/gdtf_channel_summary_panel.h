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

#include "gdtf/gdtf_modes_panel.h"

#include <vector>

#include <wx/panel.h>

class wxTextCtrl;

class GdtfChannelSummaryPanel : public wxPanel {
public:
  explicit GdtfChannelSummaryPanel(wxWindow *parent);

  void SetChannels(const std::vector<GdtfModeChannelPresentation> &channels);
  void ClearChannels();

private:
  wxTextCtrl *channelListCtrl = nullptr;
};
