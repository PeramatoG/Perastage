/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/dmx_patch_panel.h"
#include <wx/bmpbuttn.h>
#include <wx/listctrl.h>
#include <wx/popupwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>
#include <functional>

namespace {
wxString Text(const std::string &text) { return wxString::FromUTF8(text); }
wxString IssueText(dmx_patch::Issue issue) {
  using dmx_patch::Issue;
  switch (issue) {
  case Issue::Unpatched: return _("Missing patch assignment");
  case Issue::InvalidAddress: return _("Invalid or unsupported DMX address");
  case Issue::InvalidBreak: return _("Invalid or ambiguous DMX break");
  case Issue::UnresolvedDefinition: return _("GDTF definition or DMX mode unavailable");
  case Issue::UnresolvedChannel: return _("Physical channel or geometry reference unresolved");
  case Issue::Overflow: return _("Physical channels exceed 512; excess channels are not displayed");
  }
  return {};
}
class DiagnosticsPopup final : public wxPopupTransientWindow {
public:
  DiagnosticsPopup(wxWindow *parent, wxWindow *toggle, std::function<void()> close)
      : wxPopupTransientWindow(parent, wxBORDER_SIMPLE), toggle_(toggle), close_(std::move(close)) {}
  bool ProcessLeftDown(wxMouseEvent &) override {
    // Consume a toggle click before transient-popup dismissal can forward it
    // to the button and reopen the popup on the same press.
    if (toggle_->GetScreenRect().Contains(wxGetMousePosition())) {
      close_();
      return true;
    }
    return false;
  }
protected:
  void OnDismiss() override { Destroy(); }
private:
  wxWindow *toggle_;
  std::function<void()> close_;
};
}
void DmxPatchPanel::ShowDiagnostics() {
  if (diagnosticsPopup_) {
    diagnosticsPopup_->Dismiss(); diagnosticsPopup_->Destroy(); diagnosticsPopup_ = nullptr;
    return;
  }
  if (!snapshot_) return;
  auto *popup = new DiagnosticsPopup(this, warnings_, [this] { ShowDiagnostics(); });
  diagnosticsPopup_ = popup;
  popup->SetName(_("Patch diagnostics"));
  auto *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(new wxStaticText(popup, wxID_ANY, _("Patch diagnostics")), 0, wxALL, FromDIP(6));
  auto *list = new wxListView(popup, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(650, 260)), wxLC_REPORT);
  list->AppendColumn(_("Fixture"), wxLIST_FORMAT_LEFT, FromDIP(180));
  list->AppendColumn(_("Warning"), wxLIST_FORMAT_LEFT, FromDIP(280));
  list->AppendColumn(_("Details"), wxLIST_FORMAT_LEFT, FromDIP(220));
  const auto snapshot = snapshot_;
  auto add = [&](const std::string &uuid, const wxString &warning, const wxString &detail) {
    const auto &fixture = snapshot->fixtures.at(uuid);
    const auto row = list->InsertItem(list->GetItemCount(), Text(fixture.identifier + " " + fixture.name));
    list->SetItem(row, 1, warning);
    wxString details = detail;
    if (!details.empty()) details += " | ";
    details += Text(uuid);
    list->SetItem(row, 2, details);
  };
  for (const auto &issue : snapshot->diagnostics) {
    wxString details = Text(issue.detail);
    if (issue.source) {
      const auto &source = *issue.source;
      details = wxString::Format(_("Offsets: %s | GDTF break: %s | Geometry: %s"),
          Text(source.offsets), Text(source.breakId), Text(source.geometry));
      if (issue.issue == dmx_patch::Issue::Unpatched && !issue.detail.empty())
        details += " | " + wxString::Format(_("MVR break %s has no start address"), Text(issue.detail));
    }
    add(issue.uuid, IssueText(issue.issue), details);
  }
  // Group conflict ranges instead of repeating one warning for every byte.
  for (size_t first = 0; first < snapshot->cells.size();) {
    const auto &cell = snapshot->cells[first];
    if (!cell.Conflict()) { ++first; continue; }
    std::set<std::string> uuids;
    for (const auto &entry : cell.occupants) uuids.insert(entry.uuid);
    size_t end = first + 1;
    while (end < snapshot->cells.size()) {
      std::set<std::string> next;
      for (const auto &entry : snapshot->cells[end].occupants) next.insert(entry.uuid);
      if (next != uuids) break;
      ++end;
    }
    const auto detail = wxString::Format(_("Universe %s, channels %d-%d"),
        Text(std::to_string(snapshot->universe)), static_cast<int>(first + 1), static_cast<int>(end));
    for (const auto &uuid : uuids) add(uuid, _("Overlapping fixtures"), detail);
    first = end;
  }
  sizer->Add(list, 1, wxEXPAND | wxALL, FromDIP(6));
  popup->SetSizerAndFit(sizer);
  popup->Position(warnings_->ClientToScreen(wxPoint(0, 0)), warnings_->GetSize());
  popup->Popup(list);
}
