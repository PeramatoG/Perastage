#pragma once
#include "projectutils.h"
#include "typography/font_resources.h"
#include <algorithm>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace gui {
class DocumentFontChoice : public wxChoice {
public:
  DocumentFontChoice(wxWindow *parent, std::string original)
      : wxChoice(parent, wxID_ANY), original_(std::move(original)) {
    SetMinSize(wxSize(165, -1));
    Populate(original_);
    Bind(wxEVT_CHOICE, [this](wxCommandEvent &event) {
      UpdateCapability(GetFamily());
      event.Skip();
    });
  }

  void RefreshFamilies(std::string_view family) { Populate(std::string(family)); }

  static void RefreshCatalog() {
    typography::GetFontCatalog(ProjectUtils::GetResourceRoot(), typography::FontRefresh::All);
  }

  std::string GetFamily() const {
    const auto index = GetSelection();
    if (index == wxNOT_FOUND) return requested_;
    if (original_.empty() && families_[index] == typography::kDefaultFamily) return {};
    return families_[index];
  }

  std::string GetReference() const {
    const auto family = GetFamily();
    if (GetSelection() == wxNOT_FOUND || typography::DecodeFontReference(requested_).family == family)
      return requested_;
    if (const auto saved = catalog_->Resolve(requested_, false);
        saved.issue == typography::FontIssue::None && saved.face && saved.face->family == family)
      return requested_;
    return catalog_->ReferenceForFamily(family);
  }

  void SelectFamily(std::string_view family) {
    // Preserve an exact saved reference when the unavailable face is shown.
    requested_ = family;
    family = typography::DecodeFontReference(family).family;
    if (family.empty() || family == "Noto Sans") family = typography::kDefaultFamily;
    auto entry = std::find(families_.begin(), families_.end(), family);
    if (entry == families_.end()) {
      const auto resolved = catalog_->Resolve(requested_, false);
      if (resolved.issue == typography::FontIssue::None && resolved.face)
        entry = std::find(families_.begin(), families_.end(), resolved.face->family);
    }
    SetSelection(entry == families_.end() ? wxNOT_FOUND : static_cast<int>(entry - families_.begin()));
    UpdateCapability(requested_);
  }

private:
  void Populate(const std::string &family) {
    Clear();
    catalog_ = typography::GetFontCatalog(ProjectUtils::GetResourceRoot());
    families_.clear();
    for (const auto name : catalog_->Families()) {
      families_.emplace_back(name);
      const auto label = wxString::FromUTF8(name.data(), name.size());
      Append(name == typography::kDefaultFamily ? wxString::Format(_("%s (default)"), label) : label);
    }
    for (auto &name : catalog_->SystemFamilies()) {
      Append(wxString::Format(_("%s (installed)"), wxString::FromUTF8(name)));
      families_.push_back(std::move(name));
    }
    SelectFamily(family);
  }

  void UpdateCapability(std::string_view name) {
    const auto face = catalog_->Resolve(name, false);
    const auto pdf = catalog_->Resolve(name, false, false, typography::FontUsage::Pdf);
    if (face.issue == typography::FontIssue::UnsupportedFormat) {
      SetToolTip(_("This installed font format is unsupported. The document uses an approved fallback."));
    } else if (face.issue != typography::FontIssue::None) {
      SetToolTip(_("The requested font or style is unavailable. The document uses a fallback."));
    } else if (pdf.issue != typography::FontIssue::None) {
      SetToolTip(_("PDF export uses a fallback for this installed font."));
    } else if (face.face && face.face->source == typography::FontSource::InstalledSystem) {
      SetToolTip(_("Installed font. Other computers need a compatible face. Check its license before exporting."));
    } else {
      SetToolTip(_("Font family"));
    }
  }

  std::string original_, requested_;
  std::vector<std::string> families_;
  std::shared_ptr<const typography::FontCatalog> catalog_;
};

inline wxSizer *DocumentFontRow(wxWindow *parent, DocumentFontChoice *choice) {
  auto *row = new wxBoxSizer(wxHORIZONTAL);
  row->Add(new wxStaticText(parent, wxID_ANY, _("Font family")), 0,
           wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  row->Add(choice, 1);
  auto *refresh = new wxButton(parent, wxID_ANY, _("Refresh fonts"));
  row->Add(refresh, 0, wxLEFT, 8);
  refresh->Bind(wxEVT_BUTTON, [choice](wxCommandEvent &) {
    const auto requested = choice->GetReference();
    DocumentFontChoice::RefreshCatalog();
    choice->RefreshFamilies(requested);
  });
  return row;
}
}
