#include "mvr_export_result_dialog.h"

#include <map>

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {

// Returns concise user-facing copy for a diagnostic category.
wxString SummaryFor(MvrExportDiagnosticCode code, size_t count) {
  const wxString amount = wxString::FromUTF8(std::to_string(count));
  switch (code) {
  case MvrExportDiagnosticCode::GdtfFallbackUsed: return amount + " fixtures used a fallback GDTF.";
  case MvrExportDiagnosticCode::DmxAddressOmitted: return amount + " DMX addresses were omitted.";
  case MvrExportDiagnosticCode::IdentityGenerated:
  case MvrExportDiagnosticCode::IdentityReassigned:
  case MvrExportDiagnosticCode::IdentityConflict:
  case MvrExportDiagnosticCode::SymbolIdentityReplaced: return amount + " object identities were repaired.";
  case MvrExportDiagnosticCode::ForeignMetadataDiscarded: return amount + " third-party metadata blocks could not be preserved.";
  case MvrExportDiagnosticCode::TextureMissing: return amount + " model texture dependencies were omitted.";
  case MvrExportDiagnosticCode::ResourceMissing:
  case MvrExportDiagnosticCode::ResourceDuplicate: return amount + " archive resources could not be preserved.";
  case MvrExportDiagnosticCode::PlaceholderGeometryUsed:
  case MvrExportDiagnosticCode::SupportGeometryMissing: return amount + " objects used adjusted geometry.";
  case MvrExportDiagnosticCode::FixtureIdReassigned: return amount + " fixture numeric identifiers were reassigned.";
  case MvrExportDiagnosticCode::CompatibilityRepresentationUnavailable: return amount + " trusses could not use the requested compatibility representation.";
  case MvrExportDiagnosticCode::TrussGdtfMissing:
  case MvrExportDiagnosticCode::GdtfMissing:
  case MvrExportDiagnosticCode::GdtfPatchFailed: return amount + " GDTF resources could not be preserved as requested.";
  default: return amount + " export issues require attention.";
  }
}

// Builds a safely bounded result dialog with an optional details pane.
void ShowDialog(wxWindow *parent, const wxString &title,
                const wxString &primary, const wxString &summary,
                const wxString &details) {
  wxDialog dialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(720, 500),
                  wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto *layout = new wxBoxSizer(wxVERTICAL);
  auto *primaryText = new wxStaticText(&dialog, wxID_ANY, primary);
  primaryText->Wrap(660);
  layout->Add(primaryText, 0, wxALL | wxEXPAND, 14);
  if (!summary.empty()) {
    auto *summaryText = new wxStaticText(&dialog, wxID_ANY, summary);
    summaryText->Wrap(660);
    layout->Add(summaryText, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 14);
  }
  wxTextCtrl *detailText = nullptr;
  if (!details.empty()) {
    detailText = new wxTextCtrl(&dialog, wxID_ANY, details, wxDefaultPosition,
                                wxSize(-1, 240), wxTE_MULTILINE | wxTE_READONLY |
                                                    wxTE_RICH2 | wxHSCROLL);
    detailText->Hide();
    layout->Add(detailText, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 14);
  }
  auto *buttons = new wxBoxSizer(wxHORIZONTAL);
  if (detailText) {
    auto *detailsButton = new wxButton(&dialog, wxID_ANY, "Show details...");
    detailsButton->Bind(wxEVT_BUTTON, [&, detailText, detailsButton](wxCommandEvent &) {
      const bool show = !detailText->IsShown();
      detailText->Show(show);
      detailsButton->SetLabel(show ? "Hide details" : "Show details...");
      dialog.Layout();
    });
    buttons->Add(detailsButton, 0, wxRIGHT, 8);
  }
  buttons->Add(new wxButton(&dialog, wxID_OK, "OK"));
  layout->Add(buttons, 0, wxALIGN_RIGHT | wxALL, 14);
  dialog.SetSizer(layout);
  dialog.SetMinSize(wxSize(560, 260));
  dialog.CentreOnParent();
  dialog.ShowModal();
}

} // namespace

// Presents success, aggregated warnings, or a concise failure without raw logs.
void ShowMvrExportResult(wxWindow *parent, bool exported,
                         const std::vector<MvrExportDiagnostic> &diagnostics) {
  std::map<MvrExportDiagnosticCode, size_t> counts;
  wxString details;
  for (const auto &diagnostic : diagnostics) {
    if (!diagnostic.userVisible)
      continue;
    ++counts[diagnostic.code];
    if (!diagnostic.objectName.empty())
      details += wxString::FromUTF8(diagnostic.objectType + " " + diagnostic.objectName + ": ");
    details += wxString::FromUTF8(diagnostic.technicalDetail) + "\n\n";
  }
  wxString summary;
  for (const auto &[code, count] : counts)
    summary += "• " + SummaryFor(code, count) + "\n";

  if (!exported) {
    ShowDialog(parent, "MVR export failed",
               "The MVR file could not be created because export validation or file writing failed.",
               {}, details);
  } else if (!counts.empty()) {
    ShowDialog(parent, "MVR exported with warnings",
               "The MVR file was exported successfully, but some data had to be adjusted or could not be preserved.",
               summary, details);
  } else {
    ShowDialog(parent, "Success", "MVR file exported successfully.", {}, {});
  }
}
