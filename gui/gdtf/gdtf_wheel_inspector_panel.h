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

#include <wx/bitmap.h>
#include <wx/colour.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>

class wxListEvent;
class wxImageList;
class wxListCtrl;
class wxStaticBitmap;
class wxTextCtrl;

struct GdtfWheelInspectorSlotPresentation {
  std::string label;
  std::string mediaResource;
  std::string graphicResource;
  std::string rawColor;
  std::string previewStatus;
  bool selected = false;
  bool hasThumbnail = false;
  wxBitmap thumbnail;
  bool hasPreview = false;
  wxBitmap preview;
  bool hasSwatch = false;
  wxColour swatch;
};

struct GdtfWheelInspectorPresentation {
  std::string activeText;
  std::string previewStatus;
  bool hasActivePreview = false;
  wxBitmap activePreview;
  bool hasActiveSwatch = false;
  wxColour activeSwatch;
  std::vector<GdtfWheelInspectorSlotPresentation> slots;
};

class GdtfWheelInspectorPanel : public wxPanel {
public:
  explicit GdtfWheelInspectorPanel(wxWindow *parent);

  void SetPresentation(const GdtfWheelInspectorPresentation &presentation);
  void ClearPresentation();

private:
  void ApplySlotPreview(const GdtfWheelInspectorSlotPresentation &slot);
  void OnSlotSelected(wxListEvent &event);
  wxBitmap CreateSwatchBitmap(const wxColour &colour, const wxSize &size) const;
  wxBitmap CreatePlaceholderBitmap(const wxSize &size) const;

  wxTextCtrl *activeTextCtrl = nullptr;
  wxStaticBitmap *activePreviewBitmap = nullptr;
  wxTextCtrl *previewStatusCtrl = nullptr;
  wxListCtrl *slotList = nullptr;
  wxImageList *slotImages = nullptr;
  std::vector<GdtfWheelInspectorSlotPresentation> currentSlots;
};
