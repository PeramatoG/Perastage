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
#include "ridertextdialog.h"

#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/strconv.h>
#include <wx/textctrl.h>
#include <exception>
#include <filesystem>

#include "projectutils.h"
#include "consolepanel.h"
#include "riderimporter.h"
#include "ridertextpreviewdialog.h"

enum {
  ID_RiderText_Load = wxID_HIGHEST + 4200,
  ID_RiderText_Example,
  ID_RiderText_PreviewFilter,
  ID_RiderText_Apply
};

wxBEGIN_EVENT_TABLE(RiderTextDialog, wxDialog)
EVT_BUTTON(ID_RiderText_Load, RiderTextDialog::OnLoadFromFile)
EVT_BUTTON(ID_RiderText_Example, RiderTextDialog::OnLoadExample)
EVT_BUTTON(ID_RiderText_PreviewFilter, RiderTextDialog::OnPreviewFilter)
EVT_BUTTON(ID_RiderText_Apply, RiderTextDialog::OnApply)
wxEND_EVENT_TABLE()

RiderTextDialog::RiderTextDialog(wxWindow *parent,
                                 const wxString &initialText,
                                 const wxString &initialSource)
    : wxDialog(parent, wxID_ANY, "Create rider from text",
               wxDefaultPosition, wxSize(720, 520),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      sourceLabel(initialSource) {
  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

  wxBoxSizer *headerSizer = new wxBoxSizer(wxHORIZONTAL);
  const wxString sourceTextLabel =
      sourceLabel.empty() ? wxString("No source loaded.")
                          : wxString("Loaded: ") + sourceLabel;
  sourceText = new wxStaticText(this, wxID_ANY, sourceTextLabel);
  headerSizer->Add(sourceText, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  wxButton *loadButton = new wxButton(this, ID_RiderText_Load, "Load rider...");
  headerSizer->Add(loadButton, 0);
  wxButton *exampleButton =
      new wxButton(this, ID_RiderText_Example, "Use example");
  headerSizer->Add(exampleButton, 0, wxLEFT, 8);
  mainSizer->Add(headerSizer, 0, wxEXPAND | wxALL, 8);

  textCtrl = new wxTextCtrl(this, wxID_ANY, initialText,
                            wxDefaultPosition, wxDefaultSize,
                            wxTE_MULTILINE | wxTE_RICH2);
  textCtrl->SetMinSize(wxSize(680, 360));
  mainSizer->Add(textCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  wxButton *previewButton =
      new wxButton(this, ID_RiderText_PreviewFilter, "Preview filter");
  wxButton *applyButton = new wxButton(this, ID_RiderText_Apply, "Apply");
  wxButton *cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(previewButton, 0, wxRIGHT, 8);
  buttonSizer->Add(applyButton, 0, wxRIGHT, 8);
  buttonSizer->Add(cancelButton, 0);
  mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 8);

  SetSizer(mainSizer);
  Layout();
  Centre();
}

void RiderTextDialog::OnLoadFromFile(wxCommandEvent &WXUNUSED(event)) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("misc"));
  wxFileDialog dlg(this, "Import Rider", miscDir, "",
                   "Rider files (*.txt;*.pdf)|*.txt;*.pdf",
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  const std::filesystem::path selectedPath(dlg.GetPath().ToStdWstring());
  const auto pathU8 = selectedPath.u8string();
  const std::string pathUtf8(pathU8.begin(), pathU8.end());
  std::string text;
  try {
    text = RiderImporter::LoadText(pathUtf8);
  } catch (const std::exception &ex) {
    if (ConsolePanel *console = ConsolePanel::Instance()) {
      console->AppendMessage("Rider import exception: " +
                             wxString::FromUTF8(ex.what()));
    }
    wxMessageBox("Unexpected error while loading rider file.", "Error",
                 wxICON_ERROR);
    return;
  } catch (...) {
    if (ConsolePanel *console = ConsolePanel::Instance())
      console->AppendMessage("Rider import exception: unknown error.");
    wxMessageBox("Unexpected unknown error while loading rider file.", "Error",
                 wxICON_ERROR);
    return;
  }
  if (text.empty()) {
    if (ConsolePanel *console = ConsolePanel::Instance()) {
      console->AppendMessage("Rider import: no visible text extracted from " +
                             dlg.GetFilename());
    }
    wxMessageBox("Failed to import rider.", "Error", wxICON_ERROR);
    return;
  }
  wxString loadedText = wxString::FromUTF8(text.data(), text.size());
  if (loadedText.empty() && !text.empty()) {
    loadedText = wxString::From8BitData(text.data(), text.size());
  }
  if (loadedText.empty()) {
    if (ConsolePanel *console = ConsolePanel::Instance()) {
      console->AppendMessage("Rider import: extracted text could not be "
                             "decoded for " + dlg.GetFilename());
    }
    wxMessageBox("Loaded rider text could not be decoded.", "Error",
                 wxICON_ERROR);
    return;
  }
  sourceLabel = dlg.GetFilename();
  if (sourceText)
    sourceText->SetLabel(wxString("Loaded: ") + sourceLabel);
  textCtrl->ChangeValue(loadedText);
  wxMessageBox("Rider imported successfully.", "Success", wxICON_INFORMATION);
}

void RiderTextDialog::OnLoadExample(wxCommandEvent &WXUNUSED(event)) {
  const wxString exampleText =
      "Lx1\n"
      "8 Blinder 2\n"
      "8 Spiider\n"
      "6 Megapointe\n"
      "\n"
      "lx2\n"
      "6 Megapointe\n"
      "6 Mac Viper Profile\n"
      "6 Spiider\n"
      "4 Q-7\n"
      "\n"
      "lx3\n"
      "6 Megapointe\n"
      "6 Mac Viper Profile\n"
      "6 Spiider\n"
      "4 Q-7\n"
      "\n"
      "rigging\n"
      "1 truss lx1 14 m\n"
      "1 truss lx2 12 m\n"
      "1 truss lx3 12 m\n";
  textCtrl->ChangeValue(exampleText);
  sourceLabel = "Example text";
  if (sourceText)
    sourceText->SetLabel(wxString("Loaded: ") + sourceLabel);
}

void RiderTextDialog::OnPreviewFilter(wxCommandEvent &WXUNUSED(event)) {
  wxString value = textCtrl->GetValue();
  const wxScopedCharBuffer textBuffer = value.ToUTF8();
  std::string text =
      textBuffer ? std::string(textBuffer.data(), textBuffer.length())
                 : value.ToStdString();
  if (text.empty()) {
    wxMessageBox("Rider text is empty.", "Error", wxICON_ERROR);
    return;
  }

  const std::string preview = RiderImporter::BuildFixtureFilterPreview(text);
  if (preview.empty()) {
    wxMessageBox("No fixture lines were detected with the current parser "
                 "rules.",
                 "Filtered preview", wxICON_INFORMATION);
    return;
  }

  wxString previewText = wxString::FromUTF8(preview.data(), preview.size());
  if (previewText.empty() && !preview.empty())
    previewText = wxString::From8BitData(preview.data(), preview.size());
  if (previewText.empty()) {
    wxMessageBox("Filtered preview could not be decoded.", "Error",
                 wxICON_ERROR);
    return;
  }

  RiderTextPreviewDialog previewDialog(this, previewText);
  if (previewDialog.ShowModal() == wxID_OK)
    textCtrl->ChangeValue(previewDialog.GetPreviewText());
}

void RiderTextDialog::OnApply(wxCommandEvent &WXUNUSED(event)) {
  wxString value = textCtrl->GetValue();
  const wxScopedCharBuffer textBuffer = value.ToUTF8();
  std::string text =
      textBuffer ? std::string(textBuffer.data(), textBuffer.length())
                 : value.ToStdString();
  if (text.empty()) {
    wxMessageBox("Rider text is empty.", "Error", wxICON_ERROR);
    return;
  }
  if (!RiderImporter::ImportText(text)) {
    wxMessageBox("Failed to import rider text.", "Error", wxICON_ERROR);
    return;
  }
  EndModal(wxID_OK);
}
