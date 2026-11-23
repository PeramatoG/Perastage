#include "document_font_choice.h"
#include "document_font_popup.h"
#include "projectutils.h"
#include "typography/font_resources.h"
#include <algorithm>
#include <wx/button.h>
#include <wx/intl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace gui {
DocumentFontChoice::DocumentFontChoice(wxWindow *parent, std::string original)
    : wxComboCtrl(parent, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                  wxCB_READONLY),
      implicitDefault_(original.empty()), requested_(std::move(original)) {
  EnablePopupAnimation(false);
  SetMinSize(FromDIP(wxSize(165, -1)));
  SetPopupControl(new DocumentFontPopup(*this));
  Populate();
}

DocumentFontChoice::~DocumentFontChoice() {
  preview_ = {};
  CancelBrowsing();
}

std::string DocumentFontChoice::GetFamily() const {
  if (selected_) return entries_[*selected_].family;
  const auto family = typography::DecodeFontReference(requested_).family;
  return std::string(family.empty() ? typography::kDefaultFamily : family);
}

void DocumentFontChoice::SelectFamily(std::string_view reference) {
  CancelBrowsing();
  requested_ = std::string(reference);
  auto family = typography::DecodeFontReference(requested_).family;
  if (family.empty() || family == "Noto Sans") family = typography::kDefaultFamily;
  const auto find = [this](std::string_view name) {
    return std::find_if(entries_.begin(), entries_.end(),
                        [name](const Entry &entry) { return entry.family == name; });
  };
  auto entry = find(family);
  if (entry == entries_.end()) {
    const auto resolved = catalog_->Resolve(requested_, bold_, italic_);
    if (resolved.issue == typography::FontIssue::None && resolved.face)
      entry = find(resolved.face->family);
  }
  selected_ = entry == entries_.end() ? std::nullopt :
      std::optional<size_t>(static_cast<size_t>(entry - entries_.begin()));
  ChangeValue(wxString::FromUTF8(GetFamily()));
  UpdateCapability();
}

bool DocumentFontChoice::ConfirmFamily(std::string_view family) {
  const auto entry = std::find_if(entries_.begin(), entries_.end(),
                                 [family](const Entry &entry) { return entry.family == family; });
  if (entry == entries_.end()) return false;
  CancelBrowsing();
  if (selected_ && entries_[*selected_].family == family) return true;
  const auto reference = implicitDefault_ && family == typography::kDefaultFamily
      ? std::string{} : catalog_->ReferenceForFamily(family);
  SelectFamily(reference);
  if (commit_) commit_();
  return true;
}

void DocumentFontChoice::CancelBrowsing() {
  if (IsPopupShown()) HidePopup(false);
}

void DocumentFontChoice::RefreshFamilies() {
  CancelBrowsing();
  typography::GetFontCatalog(ProjectUtils::GetResourceRoot(), typography::FontRefresh::All);
  Populate();
}

void DocumentFontChoice::Populate() {
  catalog_ = typography::GetFontCatalog(ProjectUtils::GetResourceRoot());
  entries_.clear();
  for (const auto name : catalog_->Families())
    entries_.push_back({std::string(name), Origin::Included});
  auto installed = catalog_->SystemFamilies();
  std::stable_sort(installed.begin(), installed.end(), [](const auto &left, const auto &right) {
    return wxString::FromUTF8(left).CmpNoCase(wxString::FromUTF8(right)) < 0;
  });
  for (auto &name : installed) entries_.push_back({std::move(name), Origin::Installed});
  SelectFamily(requested_);
}

void DocumentFontChoice::SetFaceStyle(bool bold, bool italic) {
  if (bold_ == bold && italic_ == italic) return;
  bold_ = bold;
  italic_ = italic;
  UpdateCapability();
}

void DocumentFontChoice::UpdateCapability() {
  const auto face = catalog_->Resolve(requested_, bold_, italic_);
  const auto pdf = catalog_->Resolve(requested_, bold_, italic_, typography::FontUsage::Pdf);
  if (face.issue == typography::FontIssue::UnsupportedFormat)
    SetToolTip(_("This installed font format is unsupported. The document uses an approved fallback."));
  else if (face.issue != typography::FontIssue::None)
    SetToolTip(_("The requested font or style is unavailable. The document uses a fallback."));
  else if (pdf.issue != typography::FontIssue::None)
    SetToolTip(_("PDF export uses a fallback for this installed font."));
  else if (face.face && face.face->source == typography::FontSource::InstalledSystem)
    SetToolTip(_("Installed font. PDF embedding is available. Check its license before exporting."));
  else
    SetToolTip(_("Included portable font. PDF embedding is available."));
}

wxSizer *DocumentFontRow(wxWindow *parent, DocumentFontChoice *choice) {
  auto *row = new wxBoxSizer(wxHORIZONTAL);
  row->Add(new wxStaticText(parent, wxID_ANY, _("Font family")), 0,
           wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  row->Add(choice, 1);
  auto *refresh = new wxButton(parent, wxID_ANY, _("Refresh fonts"));
  row->Add(refresh, 0, wxLEFT, 8);
  refresh->Bind(wxEVT_BUTTON, [choice](wxCommandEvent &) { choice->RefreshFamilies(); });
  return row;
}
}
