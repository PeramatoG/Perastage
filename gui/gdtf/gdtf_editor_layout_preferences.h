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

#include <optional>
#include <string>

#include <wx/gdicmn.h>

class ConfigManager;
class wxWindow;

namespace gui::gdtf_layout {

struct FixtureLayoutPreferences {
  wxSize dialogSize;
  double contextRatio = 0.2;
  double visualRatio = 0.75;
  double gdtfRatio = 0.45;
  double modeBrowserRatio = 0.68;
  int visualTab = 0;
};

struct TrussLayoutPreferences {
  wxSize dialogSize;
  double contextRatio = 0.25;
  double previewRatio = 0.55;
  double gdtfRatio = 0.55;
};

wxSize ClampDialogSize(wxWindow *window, wxSize requested, wxSize fallback,
                       wxSize minimum);
double ClampSplitterRatio(double ratio, double fallback);
int RatioToSash(int total, int minFirst, int minSecond, double ratio);
double SashToRatio(int sash, int total, double fallback);

FixtureLayoutPreferences LoadFixtureLayoutPreferences(ConfigManager &config,
                                                       wxWindow *window);
void SaveFixtureLayoutPreferences(ConfigManager &config,
                                  const FixtureLayoutPreferences &preferences);
TrussLayoutPreferences LoadTrussLayoutPreferences(ConfigManager &config,
                                                  wxWindow *window);
void SaveTrussLayoutPreferences(ConfigManager &config,
                                const TrussLayoutPreferences &preferences);

} // namespace gui::gdtf_layout
