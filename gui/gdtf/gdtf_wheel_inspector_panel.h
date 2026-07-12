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
class wxBoxSizer;
class wxListCtrl;
class wxScrolledWindow;
class wxStaticBitmap;
class wxStaticText;
struct GdtfWheelInspectorDetailRow {
  std::string label;
  std::string value;
};

struct GdtfWheelInspectorDetailControls {
  wxBoxSizer *rowSizer = nullptr;
  wxStaticText *label = nullptr;
  wxStaticText *value = nullptr;
};

struct GdtfWheelInspectorSlotPresentation {
  std::string label;
  std::string mediaResource;
  std::string graphicResource;
  std::string rawColor;
  std::string previewStatus;
  std::vector<GdtfWheelInspectorDetailRow> detailRows;
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
  std::vector<GdtfWheelInspectorDetailRow> detailRows;
  std::vector<GdtfWheelInspectorDetailRow> previewRows;
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

  void SetDetailRows(wxScrolledWindow *panel, wxBoxSizer *sizer,
                     std::vector<GdtfWheelInspectorDetailRow> &storedRows,
                     std::vector<GdtfWheelInspectorDetailControls> &controls,
                     const std::vector<GdtfWheelInspectorDetailRow> &rows);
  void RebuildDetailRows(wxScrolledWindow *panel, wxBoxSizer *sizer,
                         std::vector<GdtfWheelInspectorDetailControls> &controls,
                         const std::vector<GdtfWheelInspectorDetailRow> &rows);
  void UpdateDetailRowValues(wxScrolledWindow *panel,
                             std::vector<GdtfWheelInspectorDetailControls> &controls,
                             const std::vector<GdtfWheelInspectorDetailRow> &rows);
  void RewrapDetailRows(wxScrolledWindow *panel,
                        const std::vector<GdtfWheelInspectorDetailControls> &controls);
  std::vector<GdtfWheelInspectorDetailRow> BuildStatusRows(const std::string &status) const;
  std::vector<GdtfWheelInspectorDetailRow> MergeDetailRows(
      const std::vector<GdtfWheelInspectorDetailRow> &details,
      const std::string &status) const;

  wxScrolledWindow *activeDetailsPanel = nullptr;
  wxBoxSizer *activeDetailsSizer = nullptr;
  wxStaticBitmap *activePreviewBitmap = nullptr;
  wxScrolledWindow *previewDetailsPanel = nullptr;
  wxBoxSizer *previewDetailsSizer = nullptr;
  wxListCtrl *slotList = nullptr;
  wxImageList *slotImages = nullptr;
  std::vector<GdtfWheelInspectorDetailRow> activeDetailRows;
  std::vector<GdtfWheelInspectorDetailRow> previewDetailRows;
  std::vector<GdtfWheelInspectorDetailControls> activeDetailControls;
  std::vector<GdtfWheelInspectorDetailControls> previewDetailControls;
  std::vector<GdtfWheelInspectorSlotPresentation> currentSlots;
};
