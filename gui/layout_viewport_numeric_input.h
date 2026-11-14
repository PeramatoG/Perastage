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
#include <wx/numformatter.h>
#include <wx/sizer.h>
#include <wx/spinbutt.h>
#include <wx/textentry.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

// A text entry plus native arrows. Display precision never becomes the stored
// value: stepping adds to the original double (or the latest manually typed
// value), and opposite steps return exactly to that anchor. Core still validates
// every complete viewport candidate before preview or acceptance.
class LayoutViewportNumericInput {
public:
  LayoutViewportNumericInput(wxWindow &window, wxTextEntryBase &entry,
      double original, double step, int digits, double minimum, double maximum,
      const wxString &label)
      : entry_(entry), original_(original), step_(step), minimum_(minimum),
        maximum_(maximum), anchor_(original), originalText_(entry.GetValue()),
        steppedText_(originalText_), digits_(digits) {
    row_ = new wxBoxSizer(wxHORIZONTAL);
    row_->Add(&window, 1, wxEXPAND);
    arrows_ = new wxSpinButton(window.GetParent(), wxID_ANY, wxDefaultPosition,
                              wxDefaultSize, wxSP_VERTICAL | wxSP_ARROW_KEYS);
#ifdef __WXGTK__
    // Native GTK arrows are horizontal and need room for both hit targets.
    constexpr int width = 48;
#else
    constexpr int width = 20;
#endif
    arrows_->SetMinSize(window.FromDIP(wxSize(width, -1)));
    arrows_->SetName(label);
    arrows_->SetRange(-1000000, 1000000);
    arrows_->SetValue(0);
    row_->Add(arrows_, 0, wxEXPAND);
    arrows_->Bind(wxEVT_SPIN_UP, [this](wxSpinEvent &) { Step(1); });
    arrows_->Bind(wxEVT_SPIN_DOWN, [this](wxSpinEvent &) { Step(-1); });
  }
  wxSizer *Row() const { return row_; }
  void Enable(bool enabled) { arrows_->Enable(enabled); }
  bool Read(double &value) const {
    const auto text = entry_.GetValue();
    if (steppedValue_ && text == steppedText_) value = *steppedValue_;
    else if (text == originalText_) value = original_;
    else if (!text.ToDouble(&value)) return false;
    return std::isfinite(value);
  }
  void SetPreciseValue(double value) {
    anchor_ = value; steps_ = 0; manualEdit_ = false;
    steppedValue_ = value;
    steppedText_ = wxNumberFormatter::ToString(value, digits_, wxNumberFormatter::Style_NoTrailingZeroes);
    stepping_ = true; entry_.SetValue(steppedText_); stepping_ = false;
  }
  void OnTextEdited() {
    if (!stepping_) { steppedValue_.reset(); manualEdit_ = true; }
  }
  bool IsUnchanged() const {
    double value;
    return Read(value) && value == original_;
  }
private:
  void Step(int direction) {
    arrows_->SetValue(0);
    double value;
    if (!Read(value)) return;
    if (manualEdit_ || entry_.GetValue() != steppedText_) { anchor_ = value; steps_ = 0; }
    manualEdit_ = false;
    const auto nextSteps = steps_ + direction;
    const double next = std::clamp(anchor_ + nextSteps * step_, minimum_, maximum_);
    if (next == value) return;
    steps_ = nextSteps;
    if (next == minimum_ || next == maximum_) { anchor_ = next; steps_ = 0; }
    steppedValue_ = next;
    steppedText_ = wxNumberFormatter::ToString(next, digits_, wxNumberFormatter::Style_NoTrailingZeroes);
    stepping_ = true;
    entry_.SetValue(steppedText_); // Emits the existing live-preview text event.
    stepping_ = false;
  }
  wxTextEntryBase &entry_;
  double original_, step_, minimum_, maximum_, anchor_;
  wxString originalText_, steppedText_;
  int digits_;
  std::int64_t steps_ = 0;
  std::optional<double> steppedValue_;
  bool stepping_ = false, manualEdit_ = false;
  wxSpinButton *arrows_ = nullptr;
  wxSizer *row_ = nullptr;
};
