#include "gdtf_catalog_details_panel.h"
#include "gdtf_catalog_browser_model.h"
#include <wx/datetime.h>
#include <wx/intl.h>
#include <wx/sizer.h>
#include <charconv>

namespace {
wxString Display(const std::string &value) {
  return value.empty() ? wxString::FromUTF8("—") : wxString::FromUTF8(value);
}
} // namespace

GdtfCatalogDetailsPanel::GdtfCatalogDetailsPanel(wxWindow *parent)
    : wxScrolledWindow(parent) {
  SetScrollRate(0, 10);
  auto *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(new wxStaticText(this, wxID_ANY, _("Selected revision")),
             0, wxALL, 10);
  auto *grid = new wxFlexGridSizer(2, 6, 8);
  grid->AddGrowableCol(1);
  const std::array<wxString, 9> labels = {
      _("Manufacturer"), _("Fixture"), _("Revision"), _("GDTF Version"),
      _("Source"), _("Creator"), _("Created"), _("Last Modified"),
      _("File size")};
  const auto addField = [&](const wxString &label) {
    auto *caption = new wxStaticText(this, wxID_ANY, label);
    auto *value = new wxStaticText(this, wxID_ANY, {}, wxDefaultPosition,
                                    wxDefaultSize, wxST_ELLIPSIZE_END);
    value->SetMinSize(wxSize(140, -1));
    grid->Add(caption, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(value, 1, wxEXPAND);
    return value;
  };
  for (std::size_t index = 0; index < labels.size(); ++index)
    values[index] = addField(labels[index]);
  const auto addIdentifier = [&](const wxString &label) {
    auto *value = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition,
                                  wxDefaultSize, wxTE_READONLY);
    grid->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(value, 1, wxEXPAND);
    return value;
  };
  uuidValue = addIdentifier(_("UUID"));
  ridValue = addIdentifier(_("RID"));
  ratingValue = addField(_("Rating"));
  sizer->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  sizer->Add(new wxStaticText(this, wxID_ANY, _("Modes")), 0, wxALL, 10);
  modes = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                  wxSize(-1, 180), wxDV_ROW_LINES);
  modes->AppendTextColumn(_("Mode"), wxDATAVIEW_CELL_INERT, 220);
  modes->AppendTextColumn(_("DMX channels"), wxDATAVIEW_CELL_INERT, 110);
  sizer->Add(modes, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  SetSizer(sizer);
  Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
    FitInside();
    event.Skip();
  });
  ShowEntry(nullptr);
}

wxString GdtfCatalogDetailsPanel::FormatTimestamp(const std::string &timestamp) {
  if (timestamp.empty())
    return {};
  long long seconds = 0;
  const auto parsed = std::from_chars(timestamp.data(),
                                      timestamp.data() + timestamp.size(), seconds);
  if (parsed.ec == std::errc{} && parsed.ptr == timestamp.data() + timestamp.size()) {
    if (seconds <= 0)
      return {};
    if (seconds > 1000000000000LL)
      seconds /= 1000;
    wxDateTime date(static_cast<time_t>(seconds));
    if (date.IsValid())
      return date.ToUTC().FormatISOCombined(' ');
  }
  return wxString::FromUTF8(timestamp);
}

void GdtfCatalogDetailsPanel::ShowEntry(
    const mvr::gdtf_catalog_matcher::GdtfCatalogEntry *entry) {
  for (auto *value : values) {
    value->SetLabel(Display({}));
    value->SetToolTip(wxString{});
  }
  uuidValue->ChangeValue(Display({}));
  ridValue->ChangeValue(Display({}));
  modes->DeleteAllItems();
  ratingValue->SetLabel(_("--"));
  if (entry) {
    const std::array<std::string, 9> text = {
        entry->manufacturer, entry->fixtureName, entry->revision, entry->version,
        entry->uploader, entry->creator,
        FormatTimestamp(entry->creationDate).ToStdString(wxConvUTF8),
        FormatTimestamp(entry->lastModifiedText.empty() && entry->lastModifiedUnix > 0
                            ? std::to_string(entry->lastModifiedUnix)
                            : entry->lastModifiedText).ToStdString(wxConvUTF8),
        gdtf_catalog_browser::FormatFileSize(entry->fileSizeBytes)};
    for (std::size_t index = 0; index < text.size(); ++index) {
      values[index]->SetLabel(Display(text[index]));
      values[index]->SetToolTip(Display(text[index]));
    }
    uuidValue->ChangeValue(Display(entry->uuid));
    ridValue->ChangeValue(Display(entry->rid));
    if (const auto rating = gdtf_catalog_browser::MeaningfulRating(entry->ratingText)) {
      ratingValue->SetLabel(wxString::FromUTF8(*rating));
    }
    for (const auto &mode : entry->modes) {
      wxVector<wxVariant> row;
      row.push_back(Display(mode.name));
      row.push_back(mode.footprint > 0 ? wxString::Format("%d", mode.footprint) : Display({}));
      modes->AppendItem(row);
    }
  }
  Layout();
  FitInside();
}
