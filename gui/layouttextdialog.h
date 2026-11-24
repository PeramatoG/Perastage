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

#include <wx/dialog.h>
#include <optional>

class wxBitmapBundle;
class wxCheckBox;
class wxColourPickerCtrl;
class wxRichTextCtrl;
class wxSpinCtrl;

class LayoutTextDialog : public wxDialog {
public:
  LayoutTextDialog(wxWindow *parent, const wxString &initialRichText,
                   const wxString &fallbackText, bool solidBackground,
                   bool drawFrame, wxSize documentSize = wxSize(480, 240),
                   std::optional<bool> wrapText = {},
                   bool useRichTextColors = false);

  wxString GetRichText() const;
  wxString GetPlainText() const;
  bool GetSolidBackground() const;
  bool GetDrawFrame() const;
  std::optional<bool> GetWrapText() const;

private:
  wxString SerializeBuffer() const;
  wxBitmapBundle LoadIcon(const std::string &name) const;
  void LoadInitialContent(const wxString &initialRichText,
                          const wxString &fallbackText);
  void ShowDocumentPreview(const wxSize &documentSize);
  void ApplyTextColor(const wxColour &color);
  void RefreshEditorBackground();
  void ApplyBold();
  void ApplyFontSize(int size);
  void AdjustFontSize(int delta);
  void ApplyAlignment(int alignment);

  wxString originalXml_, loadedXml_, legacyXmlTail_;
  std::optional<bool> wrapText_;
  bool useRichTextColors_;
  wxColourPickerCtrl *colorCtrl_ = nullptr;
  wxRichTextCtrl *textCtrl = nullptr;
  wxSpinCtrl *fontSizeCtrl = nullptr;
  wxCheckBox *solidBackgroundCtrl = nullptr;
  wxCheckBox *drawFrameCtrl = nullptr;
};
