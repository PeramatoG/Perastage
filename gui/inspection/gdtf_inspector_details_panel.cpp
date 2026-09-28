#include "inspection/gdtf_inspector_details_panel.h"

#include "gdtf/gdtf_mode_browser_presenter.h"

#include <unordered_map>

#include <wx/choice.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

namespace gui::inspection {
namespace {

// Converts retained UTF-8 model text for native presentation.
wxString Text(const std::string &value) { return wxString::FromUTF8(value); }

// Presents an explicitly unavailable optional value without inventing data.
wxString OptionalText(const std::string &value) {
  return value.empty() ? _("Unavailable") : Text(value);
}

// Appends one labelled overview value.
void AppendValue(wxString &target, const wxString &label,
                 const wxString &value) {
  target << label << ": " << value << '\n';
}

// Formats one optional authored number without treating zero as presence.
wxString AuthoredNumber(bool present, float value, const wxString &unit) {
  return present ? wxString::Format("%g %s", value, unit.c_str()) : _("Unavailable");
}

} // namespace

// Creates the read-only detail notebook used only by the Inspector.
GdtfInspectorDetailsPanel::GdtfInspectorDetailsPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  BuildLayout();
}

// Builds focused read-only pages without editor sessions or mutation actions.
void GdtfInspectorDetailsPanel::BuildLayout() {
  auto *root = new wxBoxSizer(wxVERTICAL);
  pages_ = new wxNotebook(this, wxID_ANY);
  overview_ = new wxTextCtrl(pages_, wxID_ANY, {}, wxDefaultPosition,
                             wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  auto *modePage = new wxPanel(pages_);
  auto *modeSizer = new wxBoxSizer(wxVERTICAL);
  modes_ = new wxChoice(modePage, wxID_ANY);
  modeDetails_ = new wxTextCtrl(modePage, wxID_ANY, {}, wxDefaultPosition,
                                wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY);
  modeSizer->Add(modes_, 0, wxEXPAND | wxBOTTOM, 6);
  modeSizer->Add(modeDetails_, 1, wxEXPAND);
  modePage->SetSizer(modeSizer);
  wheels_ = new wxTextCtrl(pages_, wxID_ANY, {}, wxDefaultPosition,
                           wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  pages_->AddPage(overview_, _("Overview / FixtureType"));
  pages_->AddPage(modePage, _("Modes and channels"));
  pages_->AddPage(wheels_, _("Wheels / resources"));
  root->Add(pages_, 1, wxEXPAND);
  SetSizer(root);
  modes_->Bind(wxEVT_CHOICE, [this](wxCommandEvent &event) {
    SelectMode(event.GetSelection());
  });
}

// Projects neutral Core detail models without reparsing or changing the GDTF.
void GdtfInspectorDetailsPanel::SetResult(
    const perastage::inspection::GdtfInspectionResult &result) {
  wxString overview;
  if (result.document) {
    const auto &value = result.document->Description();
    AppendValue(overview, "DataVersion", OptionalText(value.dataVersion));
    AppendValue(overview, _("Fixture type"), OptionalText(value.fixtureTypeName));
    AppendValue(overview, _("Fixture type ID"), OptionalText(value.fixtureTypeId));
    AppendValue(overview, _("Manufacturer"), OptionalText(value.manufacturer));
    AppendValue(overview, _("Short name"), OptionalText(value.shortName));
    AppendValue(overview, _("Long name"), OptionalText(value.longName));
    AppendValue(overview, _("Description"), OptionalText(value.description));
    AppendValue(overview, _("Thumbnail reference"), OptionalText(value.thumbnail));
    AppendValue(overview, _("Creation date"), OptionalText(value.createDate));
    AppendValue(overview, _("Revision"), OptionalText(value.revision));
    AppendValue(overview, _("Weight"), AuthoredNumber(value.weightKgPresent,
                                                       value.weightKg, "kg"));
    AppendValue(overview, _("Power consumption"),
                AuthoredNumber(value.powerConsumptionWPresent,
                               value.powerConsumptionW, "W"));
    AppendValue(overview, "ModelColor", OptionalText(value.modelColorHex));
    AppendValue(overview, _("Truss cross-section type"),
                OptionalText(value.trussCrossSectionType));
    AppendValue(overview, _("Truss cross-section"),
                OptionalText(value.trussCrossSection));
    overview << '\n' << _("Revision history") << ":\n";
    if (value.revisions.empty())
      overview << _("Unavailable") << '\n';
    for (const auto &revision : value.revisions)
      overview << "• " << OptionalText(revision.text) << " — "
               << OptionalText(revision.date) << " — "
               << OptionalText(revision.modifiedBy) << '\n';
  } else {
    overview = _("FixtureType metadata is unavailable.");
  }
  overview_->SetValue(overview);

  modeDocument_ = result.modeChannels;
  modes_->Clear();
  if (modeDocument_) {
    for (const auto &mode : modeDocument_->modes)
      modes_->Append(OptionalText(mode.name));
  }
  if (modes_->GetCount() > 0) {
    modes_->SetSelection(0);
    SelectMode(0);
  } else {
    modeDetails_->SetValue(_("No DMX modes are available."));
  }

  wxString wheels;
  if (result.wheelCatalog) {
    for (const auto &wheel : result.wheelCatalog->wheels) {
      wheels << _("Wheel") << ": " << OptionalText(wheel.name) << '\n';
      AppendValue(wheels, _("Type"), OptionalText(wheel.type));
      for (const auto &slot : wheel.slots) {
        wheels << "  [" << slot.index << "] " << OptionalText(slot.name)
               << '\n';
        AppendValue(wheels, "    " + _("Color"), OptionalText(slot.rawColor));
        AppendValue(wheels, "    " + _("Filter"), OptionalText(slot.rawFilter));
        AppendValue(wheels, "    " + _("Media reference"),
                    OptionalText(slot.mediaFileName));
        AppendValue(wheels, "    " + _("Archive resource"),
                    OptionalText(slot.resolvedResourcePath));
        AppendValue(wheels, "    " + _("Graphic wheel reference"),
                    OptionalText(slot.graphicWheelReference));
      }
      wheels << '\n';
    }
    for (const auto &filter : result.wheelCatalog->filters)
      wheels << _("Filter") << ": " << OptionalText(filter.name)
             << " — " << _("Color") << ": " << OptionalText(filter.rawColor)
             << '\n';
  }
  if (wheels.empty())
    wheels = _("No wheel or filter details are available.");
  wheels_->SetValue(wheels);
}

// Selects and formats one mode through the established presentation helper.
void GdtfInspectorDetailsPanel::SelectMode(int selection) {
  if (!modeDocument_ || selection < 0 ||
      static_cast<std::size_t>(selection) >= modeDocument_->modes.size())
    return;
  const auto &mode = modeDocument_->modes[static_cast<std::size_t>(selection)];
  wxString text;
  AppendValue(text, _("Description"), OptionalText(mode.description));
  AppendValue(text, _("Geometry"), OptionalText(mode.geometry));
  AppendValue(text, _("Calculated footprint"),
              wxString::Format("%d", mode.calculatedFootprint));
  text << '\n';
  const auto rows = BuildGdtfModeBrowserPresentation(&mode);
  std::unordered_map<std::string, int> depths;
  for (const auto &row : rows) {
    const int depth = row.parentId.empty() ? 0 : depths[row.parentId] + 1;
    depths[row.id] = depth;
    text << wxString(' ', depth * 2) << "• " << Text(row.item);
    if (!row.dmxRange.empty()) text << "  DMX " << Text(row.dmxRange);
    if (!row.physicalRange.empty()) text << "  " << Text(row.physicalRange);
    if (!row.unit.empty()) text << " " << Text(row.unit);
    text << '\n';
    for (const auto &detail : row.details)
      text << wxString(' ', depth * 2 + 4) << Text(detail.key) << ": "
           << Text(detail.value) << '\n';
  }
  modeDetails_->SetValue(text);
}

// Returns the useful persisted GDTF detail page selection.
int GdtfInspectorDetailsPanel::SelectedPage() const {
  return pages_->GetSelection();
}

// Restores a valid persisted GDTF detail page selection.
void GdtfInspectorDetailsPanel::SetSelectedPage(int page) {
  if (page >= 0 && page < static_cast<int>(pages_->GetPageCount()))
    pages_->SetSelection(page);
}

} // namespace gui::inspection
