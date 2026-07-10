/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "gdtf/gdtf_editor_layout_preferences.h"

#include "configmanager.h"
#include "gdtf/gdtf_editor_visual_metrics.h"

#include <algorithm>
#include <cstdlib>

#include <wx/display.h>
#include <wx/window.h>

namespace gui::gdtf_layout {
namespace {
constexpr const char *kFixtureWidth = "gdtf_editor/fixture/dialog_width";
constexpr const char *kFixtureHeight = "gdtf_editor/fixture/dialog_height";
constexpr const char *kFixtureContext = "gdtf_editor/fixture/context_ratio";
constexpr const char *kFixtureVisual = "gdtf_editor/fixture/visual_ratio";
constexpr const char *kFixtureGdtf = "gdtf_editor/fixture/gdtf_ratio";
constexpr const char *kFixtureVisualTab = "gdtf_editor/fixture/visual_tab";
constexpr const char *kTrussWidth = "gdtf_editor/truss/dialog_width";
constexpr const char *kTrussHeight = "gdtf_editor/truss/dialog_height";
constexpr const char *kTrussContext = "gdtf_editor/truss/context_ratio";
constexpr const char *kTrussPreview = "gdtf_editor/truss/preview_ratio";
constexpr const char *kTrussGdtf = "gdtf_editor/truss/gdtf_ratio";

// Parses a persisted double with a safe fallback.
double ReadDouble(ConfigManager &config, const char *key, double fallback) {
  auto value = config.GetValue(key);
  if (!value)
    return fallback;
  char *end = nullptr;
  const double parsed = std::strtod(value->c_str(), &end);
  return end && *end == '\0' ? parsed : fallback;
}

// Parses a persisted integer with a safe fallback.
int ReadInt(ConfigManager &config, const char *key, int fallback) {
  auto value = config.GetValue(key);
  if (!value)
    return fallback;
  char *end = nullptr;
  const long parsed = std::strtol(value->c_str(), &end, 10);
  return end && *end == '\0' ? static_cast<int>(parsed) : fallback;
}

// Stores one numeric preference value.
void Write(ConfigManager &config, const char *key, const std::string &value) {
  config.SetValue(key, value);
}

// Returns the current display work area or a conservative fallback.
wxRect WorkArea(wxWindow *window) {
  const int displayIndex = window ? wxDisplay::GetFromWindow(window) : wxNOT_FOUND;
  wxDisplay display(displayIndex == wxNOT_FOUND ? 0 : displayIndex);
  if (display.IsOk())
    return display.GetClientArea();
  return wxRect(0, 0, 1366, 768);
}
} // namespace

// Clamps a dialog size to the current display work area and minimum intent.
wxSize ClampDialogSize(wxWindow *window, wxSize requested, wxSize fallback,
                       wxSize minimum) {
  if (requested.GetWidth() <= 0 || requested.GetHeight() <= 0)
    requested = fallback;
  const wxRect area = WorkArea(window);
  const int maxWidth = std::max(1, area.GetWidth() - Dip(window, 24));
  const int maxHeight = std::max(1, area.GetHeight() - Dip(window, 48));
  const int minWidth = std::min(minimum.GetWidth(), maxWidth);
  const int minHeight = std::min(minimum.GetHeight(), maxHeight);
  return wxSize(std::clamp(requested.GetWidth(), minWidth, maxWidth),
                std::clamp(requested.GetHeight(), minHeight, maxHeight));
}

// Clamps a normalized splitter ratio to a non-collapsing range.
double ClampSplitterRatio(double ratio, double fallback) {
  return ClampRatio(ratio, fallback);
}

// Loads fixture editor layout preferences from GUI configuration.
FixtureLayoutPreferences LoadFixtureLayoutPreferences(ConfigManager &config,
                                                       wxWindow *window) {
  FixtureLayoutPreferences preferences;
  const wxRect area = WorkArea(window);
  preferences.dialogSize = ClampDialogSize(
      window, wxSize(ReadInt(config, kFixtureWidth, 0),
                     ReadInt(config, kFixtureHeight, 0)),
      wxSize(static_cast<int>(area.GetWidth() * 0.88),
             static_cast<int>(area.GetHeight() * 0.88)),
      wxSize(Dip(window, 1100), Dip(window, 700)));
  preferences.contextRatio = ClampSplitterRatio(ReadDouble(config, kFixtureContext, 0.2), 0.2);
  preferences.visualRatio = ClampSplitterRatio(ReadDouble(config, kFixtureVisual, 0.75), 0.75);
  preferences.gdtfRatio = ClampSplitterRatio(ReadDouble(config, kFixtureGdtf, 0.45), 0.45);
  preferences.visualTab = std::clamp(ReadInt(config, kFixtureVisualTab, 0), 0, 2);
  return preferences;
}

// Saves fixture editor layout preferences to GUI configuration.
void SaveFixtureLayoutPreferences(ConfigManager &config,
                                  const FixtureLayoutPreferences &preferences) {
  Write(config, kFixtureWidth, std::to_string(preferences.dialogSize.GetWidth()));
  Write(config, kFixtureHeight, std::to_string(preferences.dialogSize.GetHeight()));
  Write(config, kFixtureContext, std::to_string(ClampSplitterRatio(preferences.contextRatio, 0.2)));
  Write(config, kFixtureVisual, std::to_string(ClampSplitterRatio(preferences.visualRatio, 0.75)));
  Write(config, kFixtureGdtf, std::to_string(ClampSplitterRatio(preferences.gdtfRatio, 0.45)));
  Write(config, kFixtureVisualTab, std::to_string(std::clamp(preferences.visualTab, 0, 2)));
  config.SaveUserConfig();
}

// Loads truss editor layout preferences from GUI configuration.
TrussLayoutPreferences LoadTrussLayoutPreferences(ConfigManager &config,
                                                  wxWindow *window) {
  TrussLayoutPreferences preferences;
  const wxRect area = WorkArea(window);
  preferences.dialogSize = ClampDialogSize(
      window, wxSize(ReadInt(config, kTrussWidth, 0),
                     ReadInt(config, kTrussHeight, 0)),
      wxSize(static_cast<int>(area.GetWidth() * 0.8),
             static_cast<int>(area.GetHeight() * 0.8)),
      wxSize(Dip(window, 900), Dip(window, 650)));
  preferences.contextRatio = ClampSplitterRatio(ReadDouble(config, kTrussContext, 0.25), 0.25);
  preferences.previewRatio = ClampSplitterRatio(ReadDouble(config, kTrussPreview, 0.55), 0.55);
  preferences.gdtfRatio = ClampSplitterRatio(ReadDouble(config, kTrussGdtf, 0.55), 0.55);
  return preferences;
}

// Saves truss editor layout preferences to GUI configuration.
void SaveTrussLayoutPreferences(ConfigManager &config,
                                const TrussLayoutPreferences &preferences) {
  Write(config, kTrussWidth, std::to_string(preferences.dialogSize.GetWidth()));
  Write(config, kTrussHeight, std::to_string(preferences.dialogSize.GetHeight()));
  Write(config, kTrussContext, std::to_string(ClampSplitterRatio(preferences.contextRatio, 0.25)));
  Write(config, kTrussPreview, std::to_string(ClampSplitterRatio(preferences.previewRatio, 0.55)));
  Write(config, kTrussGdtf, std::to_string(ClampSplitterRatio(preferences.gdtfRatio, 0.55)));
  config.SaveUserConfig();
}

} // namespace gui::gdtf_layout
