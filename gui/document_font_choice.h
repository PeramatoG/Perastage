#pragma once
#include "projectutils.h"
#include "typography/font_resources.h"
#include <algorithm>
#include <wx/choice.h>
#include <wx/font.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace gui {
inline std::vector<std::string_view> PopulateDocumentFontChoice(wxChoice &choice) {
  const auto catalog = typography::GetFontCatalog(ProjectUtils::GetResourceRoot());
  auto families = catalog->Families();
  for (auto family : families) {
    const auto name = wxString::FromUTF8(family.data(), family.size());
    choice.Append(family == typography::kDefaultFamily
                      ? wxString::Format(_("%s (default)"), name)
                      : name);
  }
  return families;
}

class DocumentFontChoice : public wxChoice {
public:
  DocumentFontChoice(wxWindow *parent, std::string original)
      : wxChoice(parent, wxID_ANY), original_(std::move(original)),
        families_(PopulateDocumentFontChoice(*this)) {
    SelectFamily(original_);
    if (GetSelection() == wxNOT_FOUND && !original_.empty())
      SetToolTip(_("The requested font is unavailable. The document uses an approved fallback."));
  }

  void RefreshFamilies(std::string_view family) {
    Clear();
    families_ = PopulateDocumentFontChoice(*this);
    SelectFamily(family);
  }

  std::string GetFamily() const {
    const auto index = GetSelection();
    if (index == wxNOT_FOUND || (original_.empty() && families_[index] == typography::kDefaultFamily))
      return original_;
    return std::string(families_[index]);
  }

  void SelectFamily(std::string_view family) {
    if (family.empty() || family == "Noto Sans") family = typography::kDefaultFamily;
    const auto entry = std::find(families_.begin(), families_.end(), family);
    SetSelection(entry == families_.end() ? wxNOT_FOUND : static_cast<int>(entry - families_.begin()));
  }

private:
  std::string original_;
  std::vector<std::string_view> families_;
};

inline wxSizer *DocumentFontRow(wxWindow *parent, DocumentFontChoice *choice) {
  auto *row = new wxBoxSizer(wxHORIZONTAL);
  row->Add(new wxStaticText(parent, wxID_ANY, _("Font family")), 0,
           wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  row->Add(choice, 1);
  return row;
}
}
