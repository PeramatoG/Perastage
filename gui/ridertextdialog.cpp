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
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/popupwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/strconv.h>
#include <wx/textctrl.h>
#include <wx/listbox.h>
#include <wx/settings.h>
#include <wx/font.h>
#include <exception>
#include <filesystem>

#include "projectutils.h"
#include "consolepanel.h"
#include "riderimporter.h"

namespace {

bool StartsWithInsensitive(const wxString &text, const wxString &prefix) {
  return text.Lower().StartsWith(prefix.Lower());
}

} // namespace

enum {
  ID_RiderText_Load = wxID_HIGHEST + 4200,
  ID_RiderText_Example,
  ID_RiderText_ApplyFilter,
  ID_RiderText_Apply
};

wxBEGIN_EVENT_TABLE(RiderTextDialog, wxDialog)
EVT_BUTTON(ID_RiderText_Load, RiderTextDialog::OnLoadFromFile)
EVT_BUTTON(ID_RiderText_Example, RiderTextDialog::OnLoadExample)
EVT_BUTTON(ID_RiderText_ApplyFilter, RiderTextDialog::OnApplyFilter)
EVT_BUTTON(ID_RiderText_Apply, RiderTextDialog::OnApply)
wxEND_EVENT_TABLE()

RiderTextDialog::RiderTextDialog(wxWindow *parent,
                                 const wxString &initialText,
                                 const wxString &initialSource)
    : wxDialog(parent, wxID_ANY, "Create scene from text",
               wxDefaultPosition, wxSize(900, 700),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      sourceLabel(initialSource) {
  SetTitle("Create from text");
  SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

  wxStaticText *titleText = new wxStaticText(this, wxID_ANY, "Create scene from text");
  wxFont titleFont = titleText->GetFont();
  titleFont.MakeBold();
  titleFont.SetPointSize(titleFont.GetPointSize() + 2);
  titleText->SetFont(titleFont);
  mainSizer->Add(titleText, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);

  wxStaticText *subtitleText = new wxStaticText(
      this, wxID_ANY,
      "Paste rider content or load a .txt/.pdf file, then refine before creating the scene.");
  subtitleText->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  mainSizer->Add(subtitleText, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
  mainSizer->Add(new wxStaticLine(this, wxID_ANY), 0,
                 wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

  wxStaticBoxSizer *sourceSizer =
      new wxStaticBoxSizer(wxHORIZONTAL, this, "Source");
  const wxString sourceTextLabel =
      sourceLabel.empty() ? wxString("No source loaded.")
                          : wxString("Loaded: ") + sourceLabel;
  sourceText = new wxStaticText(sourceSizer->GetStaticBox(), wxID_ANY, sourceTextLabel);
  sourceSizer->Add(sourceText, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  wxButton *loadButton =
      new wxButton(sourceSizer->GetStaticBox(), ID_RiderText_Load, "Load rider...");
  sourceSizer->Add(loadButton, 0);
  wxButton *exampleButton =
      new wxButton(sourceSizer->GetStaticBox(), ID_RiderText_Example, "Use example");
  sourceSizer->Add(exampleButton, 0, wxLEFT, 8);
  mainSizer->Add(sourceSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

  wxStaticBoxSizer *editorSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Rider text");
  textCtrl = new wxTextCtrl(editorSizer->GetStaticBox(), wxID_ANY, initialText,
                            wxDefaultPosition, wxDefaultSize,
                            wxTE_MULTILINE | wxTE_RICH2);
  textCtrl->SetMinSize(wxSize(680, 360));
  editorSizer->Add(textCtrl, 1, wxEXPAND | wxALL, 8);

  wxStaticText *autocompleteHelp = new wxStaticText(
      editorSizer->GetStaticBox(), wxID_ANY,
      "Autocomplete: Up/Down move, Enter/Tab accept, Esc closes suggestions. "
      "Ranking: exact > prefix > fuzzy + recent use + context.");
  autocompleteHelp->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  autocompleteHelp->Wrap(820);
  editorSizer->Add(autocompleteHelp, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
  mainSizer->Add(editorSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

  suggestionPopup = new wxPopupTransientWindow(this, wxBORDER_SIMPLE);
  wxBoxSizer *popupSizer = new wxBoxSizer(wxVERTICAL);
  suggestionList = new wxListBox(suggestionPopup, wxID_ANY);
  popupSizer->Add(suggestionList, 1, wxEXPAND);
  suggestionPopup->SetSizerAndFit(popupSizer);
  suggestionPopup->Hide();

  if (textCtrl) {
    textCtrl->Bind(wxEVT_TEXT, &RiderTextDialog::OnTextChanged, this);
    textCtrl->Bind(wxEVT_KEY_DOWN, &RiderTextDialog::OnTextKeyDown, this);
    textCtrl->Bind(wxEVT_LEFT_DOWN, &RiderTextDialog::OnTextMouseDown, this);
    textCtrl->Bind(wxEVT_RIGHT_DOWN, &RiderTextDialog::OnTextMouseDown, this);
  }
  if (suggestionList) {
    suggestionList->Bind(wxEVT_LISTBOX_DCLICK,
                         &RiderTextDialog::OnSuggestionClick, this);
  }
  autocompleteTimer.SetOwner(this);
  Bind(wxEVT_TIMER, &RiderTextDialog::OnAutocompleteTimer, this);
  Bind(wxEVT_CHAR_HOOK, &RiderTextDialog::OnDialogCharHook, this);

  wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  wxButton *filterButton =
      new wxButton(this, ID_RiderText_ApplyFilter, "Apply filter");
  wxButton *applyButton = new wxButton(this, ID_RiderText_Apply, "Create");
  wxButton *cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(filterButton, 0, wxRIGHT, 8);
  buttonSizer->Add(applyButton, 0, wxRIGHT, 8);
  buttonSizer->Add(cancelButton, 0);
  mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 10);

  SetSizer(mainSizer);
  SetMinSize(wxSize(900, 700));
  Layout();
  CentreOnScreen();
}

const std::string &RiderTextDialog::GetRiderTextUtf8() const {
  return selectedRiderTextUtf8;
}

wxString RiderTextDialog::GetLoadedFileTitle() const {
  if (!sourceLoadedFromFile)
    return wxEmptyString;

  const wxString fileTitle = wxFileName(sourceLabel).GetName();
  if (fileTitle.IsEmpty())
    return wxEmptyString;
  return fileTitle;
}

bool RiderTextDialog::TryGetCurrentText(std::string &outText) const {
  if (!textCtrl)
    return false;

  const wxString value = textCtrl->GetValue();
  const wxScopedCharBuffer textBuffer = value.ToUTF8();
  outText = textBuffer ? std::string(textBuffer.data(), textBuffer.length())
                       : value.ToStdString();
  return true;
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
  sourceLoadedFromFile = true;
  if (sourceText)
    sourceText->SetLabel(wxString("Loaded: ") + sourceLabel);
  textCtrl->ChangeValue(loadedText);
  autocompleteProvider.RefreshDynamicTerms();
}

void RiderTextDialog::OnLoadExample(wxCommandEvent &WXUNUSED(event)) {
  const wxString exampleText =
      "LX1 \n"
      "8 Blinder 2\n"
      "8 Spiider\n"
      "6 Megapointe\n"
      "\n"
      "LX2 \n"
      "6 Megapointe\n"
      "6 Mac Viper Profile\n"
      "6 Spiider\n"
      "4 Q-7\n"
      "\n"
      "LX3\n"
      "6 Spiider\n"
      "6 Megapointe\n"
      "6 Mac Viper Profile\n"
      "4 Q-7\n"
      "\n"
      "LX4 \n"
      "3 Quantum Wash\n"
      "3 Quantum Spot\n"
      "3 Quantum Wash\n"
      "\n"
      "SIDES\n"
      "8 Quantum Wash\n"
      "\n"
      "FLOOR\n"
      "4 Tour Hazer 2\n"
      "\n"
      "SCREEN\n"
      "1 Screen 8x5m\n"
      "\n"
      "RIGGING\n"
      "1 TRUSS 40X40 14 m LX1 (-2.0, 10.5) [0.6] *(Position and margin override)*\n"
      "1 TRUSS 40X40 12 m LX2 [0.8]\n"
      "1 TRUSS 40X40 12 m LX3 [1]\n"
      "1 TRUSS 40X40 12 m LX4\n"
      "1 TRUSS 40X40 6 m SIDES (1, 4) \n"
      "1 TRUSS 40X40 10 m SCREEN\n"
      "1 PIPE 12 m BACKDROP *(Pipe as scene object)*\n"
      "4 MOTOR 500Kg FOR LX1 *(Auto-distribute across LX1 target)*\n"
      "2 MOTOR 1000Kg FOR LX2\n"
      "2 MOTOR 1000Kg FOR LX3\n"
      "2 MOTOR 1000Kg FOR LX4\n"
      "4 MOTOR 1000Kg FOR SIDES\n"
      "4 MOTOR 1000Kg FOR SCREEN\n";
  textCtrl->ChangeValue(exampleText);
  sourceLabel = "Example text";
  sourceLoadedFromFile = false;
  if (sourceText)
    sourceText->SetLabel(wxString("Loaded: ") + sourceLabel);
  autocompleteProvider.RefreshDynamicTerms();
}

void RiderTextDialog::OnApplyFilter(wxCommandEvent &WXUNUSED(event)) {
  std::string text;
  if (!TryGetCurrentText(text) || text.empty()) {
    wxMessageBox("Rider text is empty.", "Error", wxICON_ERROR);
    return;
  }

  const std::string filtered = RiderImporter::BuildFixtureFilterPreview(text);
  if (filtered.empty()) {
    wxMessageBox("No fixture lines were detected with the current parser "
                 "rules after filtering.",
                 "Apply filter", wxICON_INFORMATION);
    return;
  }

  wxString filteredText = wxString::FromUTF8(filtered.data(), filtered.size());
  if (filteredText.empty() && !filtered.empty())
    filteredText = wxString::From8BitData(filtered.data(), filtered.size());
  if (filteredText.empty()) {
    wxMessageBox("Filtered text could not be decoded.", "Error",
                 wxICON_ERROR);
    return;
  }
  textCtrl->ChangeValue(filteredText);
  autocompleteProvider.RefreshDynamicTerms();
}

void RiderTextDialog::OnApply(wxCommandEvent &WXUNUSED(event)) {
  std::string text;
  if (!TryGetCurrentText(text) || text.empty()) {
    wxMessageBox("Rider text is empty.", "Error", wxICON_ERROR);
    return;
  }
  selectedRiderTextUtf8 = std::move(text);
  EndModal(wxID_OK);
}

void RiderTextDialog::OnTextChanged(wxCommandEvent &event) {
  if (!suppressAutocompleteTextEvent)
    autocompleteTimer.StartOnce(35);
  event.Skip();
}

void RiderTextDialog::OnTextKeyDown(wxKeyEvent &event) {
  if (event.ControlDown() || event.CmdDown() || event.AltDown()) {
    HideSuggestionPopup();
    event.Skip();
    return;
  }

  if (IsSuggestionPopupVisible()) {
    switch (event.GetKeyCode()) {
    case WXK_UP:
      if (suggestionList && suggestionList->GetCount() > 0) {
        const int nextSelection =
            std::max(0, suggestionList->GetSelection() - 1);
        suggestionList->SetSelection(nextSelection);
        return;
      }
      break;
    case WXK_DOWN:
      if (suggestionList && suggestionList->GetCount() > 0) {
        const int maxIndex = static_cast<int>(suggestionList->GetCount()) - 1;
        const int nextSelection =
            std::min(maxIndex, suggestionList->GetSelection() + 1);
        suggestionList->SetSelection(nextSelection);
        return;
      }
      break;
    case WXK_ESCAPE:
      HideSuggestionPopup();
      return;
    case WXK_TAB:
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER:
      if (AcceptCurrentSuggestion())
        return;
      break;
    default:
      break;
    }
  }

  event.Skip();
}

void RiderTextDialog::OnAutocompleteTimer(wxTimerEvent &WXUNUSED(event)) {
  RefreshAutocompleteSuggestions();
}

void RiderTextDialog::OnTextMouseDown(wxMouseEvent &event) {
  HideSuggestionPopup();
  event.Skip();
}

void RiderTextDialog::OnSuggestionClick(wxCommandEvent &WXUNUSED(event)) {
  AcceptCurrentSuggestion();
}

void RiderTextDialog::OnDialogCharHook(wxKeyEvent &event) {
  if (event.GetKeyCode() == WXK_ESCAPE) {
    if (IsSuggestionPopupVisible())
      HideSuggestionPopup();
    return;
  }
  event.Skip();
}

void RiderTextDialog::RefreshAutocompleteSuggestions() {
  if (!textCtrl || !suggestionPopup || !suggestionList)
    return;

  if (wxWindow::FindFocus() != textCtrl) {
    HideSuggestionPopup();
    return;
  }

  long selectionFrom = 0;
  long selectionTo = 0;
  textCtrl->GetSelection(&selectionFrom, &selectionTo);
  if (selectionFrom != selectionTo) {
    HideSuggestionPopup();
    return;
  }

  const wxString currentText = textCtrl->GetValue();
  const wxScopedCharBuffer utf8Text = currentText.ToUTF8();
  const std::string textUtf8 =
      utf8Text ? std::string(utf8Text.data(), utf8Text.length())
               : currentText.ToStdString();
  const long insertionPoint = textCtrl->GetInsertionPoint();
  const wxString leftText = currentText.Left(insertionPoint);
  const wxScopedCharBuffer leftUtf8 = leftText.ToUTF8();
  const std::string leftUtf8Text =
      leftUtf8 ? std::string(leftUtf8.data(), leftUtf8.length())
               : leftText.ToStdString();

  currentSuggestions =
      autocompleteProvider.Query(textUtf8, leftUtf8Text.size(), 10);

  if (currentSuggestions.empty()) {
    HideSuggestionPopup();
    return;
  }

  suggestionList->Clear();
  for (const RiderTextAutocompleteProvider::Suggestion &suggestion :
       currentSuggestions) {
    suggestionList->Append(wxString::FromUTF8(suggestion.displayText));
  }
  suggestionList->SetSelection(0);

  wxPoint popupAnchor = textCtrl->ClientToScreen(wxPoint(8, 8));
  const wxPoint cursorPos = textCtrl->PositionToCoords(insertionPoint);
  if (cursorPos != wxDefaultPosition) {
    popupAnchor = textCtrl->ClientToScreen(
        wxPoint(cursorPos.x, cursorPos.y + textCtrl->GetCharHeight() + 6));
  }

  const int visibleRows = std::min<int>(6, suggestionList->GetCount());
  const wxSize popupSize(std::max(280, textCtrl->GetSize().GetWidth() / 2),
                         std::max(120, visibleRows * (textCtrl->GetCharHeight() + 8)));
  suggestionPopup->SetSize(popupAnchor.x, popupAnchor.y, popupSize.GetWidth(),
                           popupSize.GetHeight());
  suggestionPopup->Popup(textCtrl);
}

void RiderTextDialog::HideSuggestionPopup() {
  if (suggestionPopup && suggestionPopup->IsShown())
    suggestionPopup->Dismiss();
}

bool RiderTextDialog::AcceptCurrentSuggestion() {
  if (!suggestionList || !IsSuggestionPopupVisible())
    return false;

  const int selection = suggestionList->GetSelection();
  if (selection == wxNOT_FOUND || selection < 0 ||
      static_cast<size_t>(selection) >= currentSuggestions.size()) {
    HideSuggestionPopup();
    return false;
  }

  ReplaceCurrentToken(currentSuggestions[static_cast<size_t>(selection)].insertText);
  autocompleteProvider.RegisterAcceptedSuggestion(
      currentSuggestions[static_cast<size_t>(selection)].insertText);
  HideSuggestionPopup();
  return true;
}

bool RiderTextDialog::IsSuggestionPopupVisible() const {
  return suggestionPopup && suggestionPopup->IsShown();
}

void RiderTextDialog::ReplaceCurrentToken(const std::string &replacement) {
  if (!textCtrl)
    return;

  const wxString text = textCtrl->GetValue();
  const wxString replacementText = wxString::FromUTF8(replacement);
  long insertionPoint = textCtrl->GetInsertionPoint();
  insertionPoint = std::max<long>(0, std::min<long>(insertionPoint, text.length()));

  long tokenStart = insertionPoint;
  while (tokenStart > 0 && !IsTokenDelimiter(text[tokenStart - 1]))
    --tokenStart;

  if (replacementText.Find(' ') != wxNOT_FOUND) {
    long bestStart = tokenStart;
    long candidateStart = tokenStart;
    while (candidateStart > 0) {
      long previousTokenEnd = candidateStart;
      while (previousTokenEnd > 0 && IsTokenDelimiter(text[previousTokenEnd - 1]))
        --previousTokenEnd;
      if (previousTokenEnd == 0)
        break;

      long previousTokenStart = previousTokenEnd;
      while (previousTokenStart > 0 &&
             !IsTokenDelimiter(text[previousTokenStart - 1])) {
        --previousTokenStart;
      }

      const wxString typedFragment =
          text.Mid(previousTokenStart, insertionPoint - previousTokenStart);
      if (typedFragment.empty() ||
          !StartsWithInsensitive(replacementText, typedFragment)) {
        break;
      }

      bestStart = previousTokenStart;
      candidateStart = previousTokenStart;
    }
    tokenStart = bestStart;
  }

  long tokenEnd = insertionPoint;
  while (tokenEnd < static_cast<long>(text.length()) &&
         !IsTokenDelimiter(text[tokenEnd])) {
    ++tokenEnd;
  }

  suppressAutocompleteTextEvent = true;
  textCtrl->Replace(tokenStart, tokenEnd, replacementText);
  textCtrl->SetInsertionPoint(tokenStart + replacementText.length());
  suppressAutocompleteTextEvent = false;
}

bool RiderTextDialog::IsTokenDelimiter(wxUniChar c) {
  switch (c.GetValue()) {
  case ' ':
  case '\t':
  case '\r':
  case '\n':
  case ',':
  case ';':
  case ':':
  case '(':
  case ')':
  case '[':
  case ']':
  case '{':
  case '}':
  case '"':
    return true;
  default:
    return false;
  }
}
