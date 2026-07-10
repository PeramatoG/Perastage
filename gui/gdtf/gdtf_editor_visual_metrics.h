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

#include <algorithm>

#include <wx/gdicmn.h>
#include <wx/window.h>

namespace gui::gdtf_layout {

// Converts a logical dialog metric to the current window DPI.
inline int Dip(wxWindow *window, int value) {
  return window ? window->FromDIP(value) : value;
}

// Returns the standard outside margin used by GDTF editor hosts.
inline int OuterMargin(wxWindow *window) { return Dip(window, 10); }

// Returns the standard gap between primary panes.
inline int PaneGap(wxWindow *window) { return Dip(window, 8); }

// Returns the standard gap between flat sections.
inline int SectionGap(wxWindow *window) { return Dip(window, 10); }

// Returns the standard internal padding for flat sections.
inline int SectionPadding(wxWindow *window) { return Dip(window, 6); }

// Returns the compact form label-to-control gap.
inline int CompactLabelGap(wxWindow *window) { return Dip(window, 5); }

// Returns the compact form row gap.
inline int CompactFieldGap(wxWindow *window) { return Dip(window, 5); }

// Returns the minimum width for fixture and truss context panes.
inline int MinimumContextPaneWidth(wxWindow *window) { return Dip(window, 260); }

// Returns the preferred initial width for compact context panes.
inline int InitialContextPaneWidth(wxWindow *window) { return Dip(window, 300); }

// Returns the minimum width for GDTF overview panes.
inline int MinimumOverviewPaneWidth(wxWindow *window) { return Dip(window, 280); }

// Returns the minimum width for GDTF workspace panes.
inline int MinimumWorkspacePaneWidth(wxWindow *window) { return Dip(window, 320); }

// Returns the minimum width for fixture visual resource panes.
inline int MinimumVisualPaneWidth(wxWindow *window) { return Dip(window, 260); }

// Returns the minimum height for preview panes.
inline int MinimumPreviewHeight(wxWindow *window) { return Dip(window, 220); }

// Returns the margin used around standard action button rows.
inline int ButtonRowMargin(wxWindow *window) { return Dip(window, 10); }

// Clamps a normalized splitter ratio to a non-collapsing range.
inline double ClampRatio(double ratio, double fallback = 0.5) {
  if (!(ratio > 0.0 && ratio < 1.0))
    ratio = fallback;
  return std::clamp(ratio, 0.15, 0.85);
}

} // namespace gui::gdtf_layout
