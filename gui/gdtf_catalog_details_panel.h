#pragma once

#include "../mvr/gdtf_catalog_matcher.h"
#include <wx/scrolwin.h>
#include <wx/dataview.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <array>

// Presentation only: all values and modes come from the public catalog snapshot.
class GdtfCatalogDetailsPanel : public wxScrolledWindow {
public:
  explicit GdtfCatalogDetailsPanel(wxWindow *parent);
  void ShowEntry(const mvr::gdtf_catalog_matcher::GdtfCatalogEntry *entry);
  static wxString FormatTimestamp(const std::string &timestamp);

private:
  std::array<wxStaticText *, 9> values{};
  wxTextCtrl *uuidValue = nullptr;
  wxTextCtrl *ridValue = nullptr;
  wxStaticText *ratingLabel = nullptr;
  wxStaticText *ratingValue = nullptr;
  wxDataViewListCtrl *modes = nullptr;
};
