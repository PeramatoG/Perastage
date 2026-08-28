#include "mvr_export_result_dialog.h"

#include <algorithm>
#include <map>

#include <wx/button.h>
#include <wx/display.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {

constexpr int kCollapsedMinimumWidth = 620;
constexpr int kExpandedPreferredWidth = 720;
constexpr int kExpandedPreferredHeight = 500;

// Clamps a dialog size to the client area of the display containing its parent.
wxSize ClampToDisplay(wxWindow *parent, wxSize requested) {
  const int displayIndex = parent ? wxDisplay::GetFromWindow(parent) : wxNOT_FOUND;
  const wxRect clientArea = displayIndex == wxNOT_FOUND
                                ? wxGetClientDisplayRect()
                                : wxDisplay(displayIndex).GetClientArea();
  requested.SetWidth(std::min(requested.GetWidth(), clientArea.GetWidth()));
  requested.SetHeight(std::min(requested.GetHeight(), clientArea.GetHeight()));
  return requested;
}

// Maps related diagnostic codes onto one user-facing summary category.
MvrExportDiagnosticCode PresentationCode(MvrExportDiagnosticCode code) {
  switch (code) {
  case MvrExportDiagnosticCode::IdentityGenerated:
  case MvrExportDiagnosticCode::IdentityReassigned:
  case MvrExportDiagnosticCode::IdentityConflict:
  case MvrExportDiagnosticCode::SymbolIdentityReplaced:
    return MvrExportDiagnosticCode::IdentityGenerated;
  case MvrExportDiagnosticCode::GdtfMissing:
  case MvrExportDiagnosticCode::TrussGdtfMissing:
  case MvrExportDiagnosticCode::GdtfPatchFailed:
    return MvrExportDiagnosticCode::GdtfMissing;
  default:
    return code;
  }
}

// Returns concise user-facing copy for a diagnostic category.
wxString SummaryFor(MvrExportDiagnosticCode code, size_t count) {
  switch (code) {
  case MvrExportDiagnosticCode::GdtfFallbackUsed: return wxString::Format(wxPLURAL("%zu fixture used a fallback GDTF.", "%zu fixtures used a fallback GDTF.", count), count);
  case MvrExportDiagnosticCode::DmxAddressOmitted: return wxString::Format(wxPLURAL("%zu DMX address was omitted.", "%zu DMX addresses were omitted.", count), count);
  case MvrExportDiagnosticCode::IdentityGenerated: return wxString::Format(wxPLURAL("%zu object identity was repaired.", "%zu object identities were repaired.", count), count);
  case MvrExportDiagnosticCode::ReferenceCleared: return wxString::Format(wxPLURAL("%zu unresolved reference was omitted.", "%zu unresolved references were omitted.", count), count);
  case MvrExportDiagnosticCode::ForeignMetadataDiscarded: return wxString::Format(wxPLURAL("%zu third-party metadata block could not be preserved.", "%zu third-party metadata blocks could not be preserved.", count), count);
  case MvrExportDiagnosticCode::TextureMissing: return wxString::Format(wxPLURAL("%zu model texture dependency was omitted.", "%zu model texture dependencies were omitted.", count), count);
  case MvrExportDiagnosticCode::ResourceMissing:
  case MvrExportDiagnosticCode::ResourceDuplicate: return wxString::Format(wxPLURAL("%zu archive resource could not be preserved.", "%zu archive resources could not be preserved.", count), count);
  case MvrExportDiagnosticCode::PlaceholderGeometryUsed:
  case MvrExportDiagnosticCode::SupportGeometryMissing: return wxString::Format(wxPLURAL("%zu object used adjusted geometry.", "%zu objects used adjusted geometry.", count), count);
  case MvrExportDiagnosticCode::FixtureIdReassigned: return wxString::Format(wxPLURAL("%zu fixture numeric identifier was reassigned.", "%zu fixture numeric identifiers were reassigned.", count), count);
  case MvrExportDiagnosticCode::CompatibilityRepresentationUnavailable: return wxString::Format(wxPLURAL("%zu truss could not use the requested compatibility representation.", "%zu trusses could not use the requested compatibility representation.", count), count);
  case MvrExportDiagnosticCode::GdtfMissing:
    return wxString::Format(wxPLURAL("%zu GDTF resource could not be preserved as requested.", "%zu GDTF resources could not be preserved as requested.", count), count);
  default: return wxString::Format(wxPLURAL("%zu export issue requires attention.", "%zu export issues require attention.", count), count);
  }
}

// Builds a safely bounded result dialog with an optional details pane.
void ShowDialog(wxWindow *parent, const wxString &title,
                const wxString &primary, const wxString &summary,
                const wxString &details) {
  wxDialog dialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
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
  wxButton *detailsButton = nullptr;
  if (detailText) {
    detailsButton = new wxButton(&dialog, wxID_ANY, _("Show details..."));
    buttons->Add(detailsButton, 0, wxRIGHT, 8);
  }
  buttons->Add(new wxButton(&dialog, wxID_OK, _("OK")));
  layout->Add(buttons, 0, wxALIGN_RIGHT | wxALL, 14);
  dialog.SetSizerAndFit(layout);
  wxSize collapsedSize = dialog.GetSize();
  collapsedSize.SetWidth(std::max(collapsedSize.GetWidth(),
                                  kCollapsedMinimumWidth));
  collapsedSize = ClampToDisplay(parent, collapsedSize);
  dialog.SetSize(collapsedSize);
  dialog.SetMinSize(collapsedSize);
  if (detailsButton) {
    detailsButton->Bind(
        wxEVT_BUTTON,
        [&, detailText, detailsButton, collapsedSize](wxCommandEvent &) {
          const bool show = !detailText->IsShown();
          detailText->Show(show);
          detailsButton->SetLabel(show ? _("Hide details") : _("Show details..."));
          if (show) {
            dialog.SetMinSize(collapsedSize);
            dialog.SetSize(ClampToDisplay(
                parent, wxSize(std::max(collapsedSize.GetWidth(),
                                        kExpandedPreferredWidth),
                               std::max(collapsedSize.GetHeight() + 280,
                                        kExpandedPreferredHeight))));
          } else {
            dialog.SetSize(collapsedSize);
          }
          dialog.Layout();
        });
  }
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
    ++counts[PresentationCode(diagnostic.code)];
    if (!diagnostic.objectName.empty())
      details += wxString::FromUTF8(diagnostic.objectType + " " + diagnostic.objectName + ": ");
    details += wxString::FromUTF8(diagnostic.technicalDetail) + "\n\n";
  }
  wxString summary;
  for (const auto &[code, count] : counts)
    summary += "- " + SummaryFor(code, count) + "\n";

  if (!exported) {
    ShowDialog(parent, _("MVR export failed"),
               _("The MVR file could not be created because export validation or file writing failed."),
               {}, details);
  } else if (!counts.empty()) {
    ShowDialog(parent, _("MVR exported with warnings"),
               _("The MVR file was exported successfully, but some data had to be adjusted or could not be preserved."),
               summary, details);
  } else {
    ShowDialog(parent, _("Success"), _("MVR file exported successfully."), {}, {});
  }
}
