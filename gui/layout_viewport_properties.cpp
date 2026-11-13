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
#include "layout_viewport_numeric_input.h"
#include <memory>
#include <vector>
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
#include <wx/numformatter.h>

// Presents paper geometry separately from scene framing and workspace zoom.
bool EditLayoutViewportProperties(wxWindow *parent,
    layouts::Layout2DViewDefinition &draft, const Viewer2DViewState &visualState,
    const std::function<void(const layouts::Layout2DViewDefinition &)> &preview) {
  const auto original = draft;
  // Controllers outlive the dialog-owned controls and their event bindings.
  std::vector<std::unique_ptr<LayoutViewportNumericInput>> numbers;
  wxDialog dialog(parent, wxID_ANY, _("Viewport properties"));
  auto *box = new wxBoxSizer(wxVERTICAL);
  auto *grid = new wxFlexGridSizer(2, 8, 8);
  grid->AddGrowableCol(1);
  auto row = [&](const wxString &label, auto *control) {
    grid->Add(new wxStaticText(&dialog, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(control, 1, wxEXPAND);
  };
  auto *mode = new wxChoice(&dialog, wxID_ANY);
  mode->Append(_("Free / Variable")); mode->Append(_("Fixed"));
  mode->SetSelection(original.scaleMode == layouts::ViewportScaleMode::Fixed ? 1 : 0);
  row(_("Scale mode"), mode);
  auto physical = original.physicalScale.value_or(layouts::PhysicalViewport{});
  viewer2d::Viewer2DViewBounds bounds;
  const bool hasBounds = viewer2d::ComputeViewBounds(visualState, bounds);
  if (!original.physicalScale && hasBounds) {
    physical.centerX = layouts::ViewportHorizontalSign(static_cast<int>(visualState.view)) * (bounds.minX + bounds.maxX) / 2;
    physical.centerY = (bounds.minY + bounds.maxY) / 2;
  }
  auto *denominator = new wxComboBox(&dialog, wxID_ANY,
      wxNumberFormatter::ToString(physical.denominator, 8, wxNumberFormatter::Style_NoTrailingZeroes));
  for (const auto *v : {"25", "50", "100", "200", "500"}) denominator->Append(v);
  auto numericRow = [&](const wxString &label, auto *field, double value,
                        double step, int digits, double minimum, double maximum) {
    numbers.push_back(std::make_unique<LayoutViewportNumericInput>(
        *field, *field, value, step, digits, minimum, maximum, label));
    row(label, numbers.back()->Row());
  };
  numericRow(_("Drawing scale 1:"), denominator, physical.denominator, 1, 8, 0.01, 1e7);
  auto text = [&](const wxString &label, double value, double step, int precision, double minimum, double maximum) {
    auto *field = new wxTextCtrl(&dialog, wxID_ANY, wxNumberFormatter::ToString(value, precision, wxNumberFormatter::Style_NoTrailingZeroes));
    numericRow(label, field, value, step, precision, minimum, maximum); return field;
  };
  auto *cx = text(_("Center horizontal (m)"), physical.centerX, 0.01, 3, -1e7, 1e7);
  auto *cy = text(_("Center vertical (m)"), physical.centerY, 0.01, 3, -1e7, 1e7);
  const auto f = layouts::ResolveViewportFrame(original);
  constexpr double mmPerPoint = 25.4 / 72.0;
  std::array<wxTextCtrl *,2> frame = {
    text(_("Frame width (mm)"), f.width * mmPerPoint, 1, 0, mmPerPoint, 200000*mmPerPoint),
    text(_("Frame height (mm)"), f.height * mmPerPoint, 1, 0, mmPerPoint, 200000*mmPerPoint)};
  box->Add(grid, 1, wxEXPAND | wxALL, 12);
  auto *axes = new wxStaticText(&dialog, wxID_ANY,
      _("Center axes: Top X/Y, Front X/Z, Side Y/Z."));
  box->Add(axes, 0, wxALL, 12);
  auto *effective = new wxStaticText(&dialog, wxID_ANY, _("Free scale: variable"));
  Viewer2DViewState freeState;
  freeState.zoom = original.camera.zoom;
  freeState.offsetPixelsX = original.camera.offsetPixelsX; freeState.offsetPixelsY = original.camera.offsetPixelsY;
  freeState.viewportWidth = original.camera.viewportWidth; freeState.viewportHeight = original.camera.viewportHeight;
  viewer2d::Viewer2DViewBounds freeBounds;
  if (viewer2d::ComputeViewBounds(freeState, freeBounds) && f.width > 0 && f.height > 0) {
    const double pointsPerMeter = std::min(f.width / freeBounds.width, f.height / freeBounds.height);
    effective->SetLabel(wxString::Format(_("Effective free scale: approximately 1:%.4f"),
        1000.0 * 72.0 / 25.4 / pointsPerMeter));
  }
  box->Add(effective, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
  auto enable = [&]() {
    bool fixed = mode->GetSelection() == 1;
    denominator->Enable(fixed); cx->Enable(fixed); cy->Enable(fixed);
    for (size_t i=0; i<3; ++i) numbers[i]->Enable(fixed);
    effective->Show(!fixed);
  };
  enable();
  box->Add(dialog.CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);
  dialog.SetSizerAndFit(box);
  auto readCandidate = [&]() {
    auto candidate = original;
    double d, x, y;
    std::array<double,2> n{};
    bool valid = numbers[0]->Read(d) && numbers[1]->Read(x) && numbers[2]->Read(y);
    for (size_t i = 0; i < n.size(); ++i) valid = numbers[i+3]->Read(n[i]) && valid;
    if (!valid) throw std::invalid_argument("Enter finite numeric values for scale, center and frame.");
    candidate.scaleMode = mode->GetSelection() == 1 ? layouts::ViewportScaleMode::Fixed : layouts::ViewportScaleMode::Free;
    if (candidate.scaleMode == layouts::ViewportScaleMode::Fixed || original.physicalScale)
      candidate.physicalScale = layouts::PhysicalViewport{d, x, y};
    // Preserve the exact stored doubles on an unchanged edit.
    const std::array<double,2> old = {f.width, f.height};
    for (size_t i = 0; i < n.size(); ++i)
      n[i] = numbers[i+3]->IsUnchanged() ? old[i] : n[i]/mmPerPoint;
    auto parsedFrame = original;
    layouts::SetViewportFrame(parsedFrame, {f.x, f.y, n[0], n[1]});
    // Keep unchanged residuals bit-for-bit, including JSON-authored fractions.
    if (!numbers[3]->IsUnchanged()) {
      candidate.frame.width=parsedFrame.frame.width; candidate.frameResidual.width=parsedFrame.frameResidual.width;
    }
    if (!numbers[4]->IsUnchanged()) {
      candidate.frame.height=parsedFrame.frame.height; candidate.frameResidual.height=parsedFrame.frameResidual.height;
    }
    layouts::ValidateViewport(candidate);
    return candidate;
  };
  auto previewValues = [&]() {
    try {
      const auto candidate = readCandidate();
      if (viewer2d::ComputeViewBounds(freeState, freeBounds)) {
        const auto geometry = layouts::ResolveViewportFrame(candidate);
        const double ppm = std::min(geometry.width/freeBounds.width, geometry.height/freeBounds.height);
        effective->SetLabel(wxString::Format(_("Effective free scale: approximately 1:%.4f"),
            1000.0*72.0/25.4/ppm));
      }
      if (preview) preview(candidate);
    } catch (const std::invalid_argument &) {
      // Incomplete typing keeps the last valid preview; OK reports diagnostics.
    }
  };
  for (size_t i=0; i<frame.size(); ++i)
    frame[i]->Bind(wxEVT_TEXT, [&, i](wxCommandEvent &) {
      numbers[i+3]->OnTextEdited(); previewValues();
    });
  cx->Bind(wxEVT_TEXT, [&](wxCommandEvent &) { numbers[1]->OnTextEdited(); previewValues(); });
  cy->Bind(wxEVT_TEXT, [&](wxCommandEvent &) { numbers[2]->OnTextEdited(); previewValues(); });
  denominator->Bind(wxEVT_TEXT, [&](wxCommandEvent &) { numbers[0]->OnTextEdited(); previewValues(); });
  denominator->Bind(wxEVT_COMBOBOX, [&](wxCommandEvent &) { previewValues(); });
  mode->Bind(wxEVT_CHOICE, [&](wxCommandEvent &) {
    enable(); dialog.Layout(); previewValues();
  });
  dialog.Bind(wxEVT_BUTTON, [&](wxCommandEvent &) {
    try {
      const auto candidate = readCandidate();
      if (preview) preview(candidate);
      draft = candidate;
      dialog.EndModal(wxID_OK);
    } catch (const std::exception &e) {
      wxMessageBox(wxString::Format(_("Invalid viewport properties: %s"), wxString::FromUTF8(e.what())),
          _("Viewport properties"), wxOK | wxICON_ERROR, &dialog);
    }
  }, wxID_OK);
  const bool accepted = dialog.ShowModal() == wxID_OK;
  if (!accepted) {
    draft = original;
    if (preview) preview(original);
  }
  return accepted;
}
