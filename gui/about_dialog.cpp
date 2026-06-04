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
#include "about_dialog.h"

#include "app_version.h"

#include <initializer_list>

#include <wx/arrstr.h>
#include <wx/artprov.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/hyperlink.h>
#include <wx/image.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>

namespace {

constexpr const char *kProjectWebsiteUrl =
    "https://perastage.luismaperamato.com/";
constexpr int kLogoSizeDip = 128;
constexpr int kDialogWidthDip = 620;

// Finds the first existing runtime asset path.
wxString
FindRuntimeAsset(const std::initializer_list<wxString> &relativePaths) {
  wxArrayString roots;
  roots.Add(wxGetCwd());

  wxFileName executable(wxStandardPaths::Get().GetExecutablePath());
  roots.Add(executable.GetPath());
  roots.Add(executable.GetPathWithSep() + "resources");
  roots.Add(executable.GetPathWithSep() + "../Resources");

  for (const wxString &root : roots) {
    for (const wxString &relativePath : relativePaths) {
      const wxString fullPath =
          wxFileName(root, wxEmptyString).GetPathWithSep() + relativePath;
      wxFileName candidate(fullPath);
      candidate.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
      if (candidate.FileExists()) {
        return candidate.GetFullPath();
      }
    }
  }

  for (const wxString &relativePath : relativePaths) {
    if (wxFileExists(relativePath)) {
      return relativePath;
    }
  }

  return wxString();
}

// Loads the Perastage logo bitmap at the requested device-independent size.
wxBitmap LoadLogoBitmap(wxWindow *parent) {
  const wxString logoPath =
      FindRuntimeAsset({"resources/Perastage_logo.png", "Perastage_logo.png"});
  const int logoSize = parent->FromDIP(kLogoSizeDip);
  if (logoPath.empty()) {
    return wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_OTHER,
                                    wxSize(logoSize, logoSize));
  }

  wxImage logoImage;
  if (!logoImage.LoadFile(logoPath, wxBITMAP_TYPE_PNG) || !logoImage.IsOk()) {
    return wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_OTHER,
                                    wxSize(logoSize, logoSize));
  }

  logoImage.Rescale(logoSize, logoSize, wxIMAGE_QUALITY_HIGH);
  return wxBitmap(logoImage);
}

// Creates a bold static text control used for section headings.
wxStaticText *CreateSectionLabel(wxWindow *parent, const wxString &label) {
  auto *text = new wxStaticText(parent, wxID_ANY, label);
  wxFont font = text->GetFont();
  font.SetWeight(wxFONTWEIGHT_BOLD);
  text->SetFont(font);
  return text;
}

// Adds a labeled text block to the dialog content column.
void AddSection(wxWindow *parent, wxBoxSizer *sizer, const wxString &label,
                const wxString &body) {
  sizer->Add(CreateSectionLabel(parent, label), 0, wxTOP, parent->FromDIP(10));
  auto *bodyText = new wxStaticText(parent, wxID_ANY, body);
  bodyText->Wrap(parent->FromDIP(380));
  sizer->Add(bodyText, 0, wxEXPAND | wxTOP, parent->FromDIP(3));
}

// Builds and returns the user-facing About dialog content panel.
wxSizer *CreateAboutContent(wxDialog *dialog) {
  auto *rootSizer = new wxBoxSizer(wxHORIZONTAL);
  rootSizer->Add(new wxStaticBitmap(dialog, wxID_ANY, LoadLogoBitmap(dialog)),
                 0, wxALIGN_TOP | wxRIGHT, dialog->FromDIP(24));

  auto *contentSizer = new wxBoxSizer(wxVERTICAL);

  const wxString titleText = wxString::FromUTF8(app::kName) + " " +
                             wxString::FromUTF8(app::kVersionDisplay);
  auto *title = new wxStaticText(dialog, wxID_ANY, titleText);
  wxFont titleFont = title->GetFont();
  titleFont.MakeBold();
  titleFont.SetPointSize(titleFont.GetPointSize() + 4);
  title->SetFont(titleFont);
  contentSizer->Add(title, 0, wxEXPAND);

  auto *description = new wxStaticText(
      dialog, wxID_ANY,
      "Free and open-source MVR viewer/editor for lighting and stage projects, "
      "with integrated 2D/3D visualization, MVR import/export, and Create from "
      "Text tools.");
  description->Wrap(dialog->FromDIP(380));
  contentSizer->Add(description, 0, wxEXPAND | wxTOP, dialog->FromDIP(8));

  contentSizer->Add(new wxStaticLine(dialog), 0, wxEXPAND | wxTOP | wxBOTTOM,
                    dialog->FromDIP(12));

  contentSizer->Add(CreateSectionLabel(dialog, "Website"), 0, wxBOTTOM,
                    dialog->FromDIP(3));
  contentSizer->Add(new wxHyperlinkCtrl(dialog, wxID_ANY, kProjectWebsiteUrl,
                                        kProjectWebsiteUrl),
                    0, wxEXPAND);

  AddSection(
      dialog, contentSizer, "License",
      "This software is licensed under the GNU General Public License v3.0.");
  AddSection(dialog, contentSizer, "Author", "Luisma Peramato");
  AddSection(
      dialog, contentSizer, "Open-source software notice",
      "Perastage is built with open-source technologies. Full dependency "
      "and license details are available in the project repository.");

  rootSizer->Add(contentSizer, 1, wxEXPAND);
  return rootSizer;
}

} // namespace

// Shows the modern Perastage About dialog.
void gui::ShowAboutDialog(wxWindow *parent) {
  wxDialog dialog(parent, wxID_ANY, "About Perastage", wxDefaultPosition,
                  wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

  auto *dialogSizer = new wxBoxSizer(wxVERTICAL);
  dialogSizer->Add(CreateAboutContent(&dialog), 1, wxEXPAND | wxALL,
                   dialog.FromDIP(18));

  auto *buttonSizer = dialog.CreateSeparatedButtonSizer(wxOK);
  if (buttonSizer) {
    dialogSizer->Add(buttonSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                     dialog.FromDIP(12));
  }

  dialog.SetSizerAndFit(dialogSizer);
  const wxSize fittedSize = dialog.GetSize();
  const int minimumWidth = dialog.FromDIP(kDialogWidthDip);
  dialog.SetMinSize(wxSize(minimumWidth, fittedSize.GetHeight()));
  if (fittedSize.GetWidth() < minimumWidth) {
    dialog.SetSize(wxSize(minimumWidth, fittedSize.GetHeight()));
  }
  dialog.CentreOnParent();
  dialog.ShowModal();
}
