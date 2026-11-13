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
#include "layout_viewport_properties.h"
#include "viewer2dpanel.h"
#include "viewer2dcommandrenderer.h"
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <array>

// Presents paper geometry separately from scene framing and workspace zoom.
bool EditLayoutViewportProperties(wxWindow *parent,
    layouts::Layout2DViewDefinition &draft, const Viewer2DViewState &visualState) {
  wxDialog dialog(parent, wxID_ANY, _("Viewport properties"));
  auto *box = new wxBoxSizer(wxVERTICAL);
  auto *grid = new wxFlexGridSizer(2, 8, 8);
  grid->AddGrowableCol(1);
  auto row = [&](const wxString &label, wxWindow *control) {
    grid->Add(new wxStaticText(&dialog, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(control, 1, wxEXPAND);
  };
  auto *mode = new wxChoice(&dialog, wxID_ANY);
  mode->Append(_("Free / Variable")); mode->Append(_("Fixed"));
  mode->SetSelection(draft.scaleMode == layouts::ViewportScaleMode::Fixed ? 1 : 0);
  row(_("Scale mode"), mode);
  auto physical = draft.physicalScale.value_or(layouts::PhysicalViewport{});
  viewer2d::Viewer2DViewBounds bounds;
  const bool hasBounds = viewer2d::ComputeViewBounds(visualState, bounds);
  if (!draft.physicalScale && hasBounds) {
    physical.centerX = layouts::ViewportHorizontalSign(static_cast<int>(visualState.view)) * (bounds.minX + bounds.maxX) / 2;
    physical.centerY = (bounds.minY + bounds.maxY) / 2;
  }
  auto *denominator = new wxComboBox(&dialog, wxID_ANY,
      wxString::FromDouble(physical.denominator, 8));
  for (const auto *v : {"25", "50", "100", "200", "500"}) denominator->Append(v);
  row(_("Drawing scale 1:"), denominator);
  auto text = [&](const wxString &label, double value) {
    auto *field = new wxTextCtrl(&dialog, wxID_ANY, wxString::FromDouble(value, 10));
    row(label, field); return field;
  };
  auto *cx = text(_("Center horizontal (m)"), physical.centerX);
  auto *cy = text(_("Center vertical (m)"), physical.centerY);
  const auto f = layouts::ResolveViewportFrame(draft);
  constexpr double mmPerPoint = 25.4 / 72.0;
  std::array<wxTextCtrl *,4> frame = {
    text(_("Page X (mm)"), f.x * mmPerPoint),
    text(_("Page Y (mm)"), f.y * mmPerPoint),
    text(_("Frame width (mm)"), f.width * mmPerPoint),
    text(_("Frame height (mm)"), f.height * mmPerPoint)};
  box->Add(grid, 1, wxEXPAND | wxALL, 12);
  auto *axes = new wxStaticText(&dialog, wxID_ANY,
      _("Center axes: Top X/Y, Front X/Z, Side Y/Z. Page origin: top left."));
  box->Add(axes, 0, wxALL, 12);
  auto *effective = new wxStaticText(&dialog, wxID_ANY, _("Free scale: variable"));
  Viewer2DViewState freeState;
  freeState.zoom = draft.camera.zoom;
  freeState.offsetPixelsX = draft.camera.offsetPixelsX; freeState.offsetPixelsY = draft.camera.offsetPixelsY;
  freeState.viewportWidth = draft.camera.viewportWidth; freeState.viewportHeight = draft.camera.viewportHeight;
  viewer2d::Viewer2DViewBounds freeBounds;
  if (viewer2d::ComputeViewBounds(freeState, freeBounds) && f.width > 0 && f.height > 0) {
    const double pointsPerMeter = std::min(f.width / freeBounds.width, f.height / freeBounds.height);
    effective->SetLabel(wxString::Format(_("Effective free scale: approximately 1:%.4f"),
        1000.0 * 72.0 / 25.4 / pointsPerMeter));
  }
  box->Add(effective, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
  auto *recenter = new wxButton(&dialog, wxID_ANY, _("Use visual framing center"));
  recenter->Enable(hasBounds);
  recenter->Bind(wxEVT_BUTTON, [=](wxCommandEvent &) {
    cx->SetValue(wxString::FromDouble(layouts::ViewportHorizontalSign(static_cast<int>(visualState.view)) * (bounds.minX + bounds.maxX) / 2, 10));
    cy->SetValue(wxString::FromDouble((bounds.minY + bounds.maxY) / 2, 10));
  });
  box->Add(recenter, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
  auto enable = [=]() {
    bool fixed = mode->GetSelection() == 1;
    denominator->Enable(fixed); cx->Enable(fixed); cy->Enable(fixed);
    recenter->Enable(fixed && hasBounds); effective->Show(!fixed);
  };
  mode->Bind(wxEVT_CHOICE, [=, &dialog](wxCommandEvent &) { enable(); dialog.Layout(); });
  enable();
  box->Add(dialog.CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);
  dialog.SetSizerAndFit(box);
  dialog.Bind(wxEVT_BUTTON, [&](wxCommandEvent &) {
    auto candidate = draft;
    double d, x, y;
    std::array<double,4> n{};
    bool valid = denominator->GetValue().ToDouble(&d) &&
                 cx->GetValue().ToDouble(&x) && cy->GetValue().ToDouble(&y);
    for (size_t i = 0; i < n.size(); ++i) valid = frame[i]->GetValue().ToDouble(&n[i]) && valid;
    try {
      if (denominator->GetValue() == wxString::FromDouble(physical.denominator, 8)) d = physical.denominator;
      if (cx->GetValue() == wxString::FromDouble(physical.centerX, 10)) x = physical.centerX;
      if (cy->GetValue() == wxString::FromDouble(physical.centerY, 10)) y = physical.centerY;
      if (!valid) throw std::invalid_argument("Enter finite numeric values for scale, center and frame.");
      candidate.scaleMode = mode->GetSelection() == 1 ? layouts::ViewportScaleMode::Fixed : layouts::ViewportScaleMode::Free;
      if (candidate.scaleMode == layouts::ViewportScaleMode::Fixed || draft.physicalScale)
        candidate.physicalScale = layouts::PhysicalViewport{d, x, y};
      // Preserve the exact stored doubles on an unchanged edit.
      bool changed = false;
      const std::array<double,4> old = {f.x, f.y, f.width, f.height};
      for (size_t i = 0; i < n.size(); ++i)
        changed = changed || std::abs(n[i] - old[i] * mmPerPoint) > 1e-9;
      if (changed) layouts::SetViewportFrame(candidate,
          {n[0]/mmPerPoint, n[1]/mmPerPoint, n[2]/mmPerPoint, n[3]/mmPerPoint});
      layouts::ValidateViewport(candidate);
      draft = candidate; dialog.EndModal(wxID_OK);
    } catch (const std::exception &e) {
      wxMessageBox(wxString::Format(_("Invalid viewport properties: %s"), wxString::FromUTF8(e.what())),
          _("Viewport properties"), wxOK | wxICON_ERROR, &dialog);
    }
  }, wxID_OK);
  return dialog.ShowModal() == wxID_OK;
}
