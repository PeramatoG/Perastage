#include "inspection/gdtf_inspector_details_panel.h"

#include "inspection/gdtf_inspector_presentation.h"

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

// Presents a projected optional value without interpreting its contents.
wxString OptionalText(const GdtfInspectorValue &value) {
  return value.available ? Text(value.value) : _("Unavailable");
}

// Maps a neutral overview field to its localized GUI label.
wxString OverviewLabel(GdtfOverviewField field) {
  switch (field) {
  case GdtfOverviewField::DataVersion: return "DataVersion";
  case GdtfOverviewField::FixtureTypeName: return _("Fixture type");
  case GdtfOverviewField::FixtureTypeId: return _("Fixture type ID");
  case GdtfOverviewField::Manufacturer: return _("Manufacturer");
  case GdtfOverviewField::ShortName: return _("Short name");
  case GdtfOverviewField::LongName: return _("Long name");
  case GdtfOverviewField::Description: return _("Description");
  case GdtfOverviewField::Thumbnail: return _("Thumbnail reference");
  case GdtfOverviewField::CreationDate: return _("Creation date");
  case GdtfOverviewField::Revision: return _("Revision");
  case GdtfOverviewField::Weight: return _("Weight");
  case GdtfOverviewField::PowerConsumption: return _("Power consumption");
  case GdtfOverviewField::ModelColor: return "ModelColor";
  case GdtfOverviewField::TrussCrossSectionType:
    return _("Truss cross-section type");
  case GdtfOverviewField::TrussCrossSection:
    return _("Truss cross-section");
  }
  return {};
}

} // namespace

// Creates the read-only detail notebook used only by the Inspector.
GdtfInspectorDetailsPanel::GdtfInspectorDetailsPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  BuildLayout();
}

// Clears all format-specific values without changing native page ownership.
void GdtfInspectorDetailsPanel::ClearResult() {
  overview_->SetValue(_("FixtureType metadata is unavailable."));
  modeDocument_.reset();
  modes_->Clear();
  modeDetails_->SetValue(_("No DMX modes are available."));
  wheels_->SetValue(_("No wheel or filter details are available."));
  pages_->ChangeSelection(0);
}

// Enables GDTF details only while a GDTF source is displayed.
void GdtfInspectorDetailsPanel::SetAvailable(bool available) {
  if (!available)
    ClearResult();
  Enable(available);
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
  Enable(true);
  wxString overview;
  if (result.document) {
    const auto presentation =
        BuildGdtfOverviewPresentation(result.document->Description());
    for (const auto &row : presentation.rows)
      AppendValue(overview, OverviewLabel(row.field), OptionalText(row.value));
    overview << '\n' << _("Revision history") << ":\n";
    if (presentation.revisions.empty())
      overview << _("Unavailable") << '\n';
    for (const auto &revision : presentation.revisions) {
      overview << "- " << OptionalText(revision.text) << " | "
               << OptionalText(revision.date) << " | UserID: "
               << OptionalText(revision.userId) << " | ModifiedBy: "
               << OptionalText(revision.modifiedBy) << '\n';
    }
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
    const auto presentation = BuildGdtfWheelsPresentation(*result.wheelCatalog);
    for (const auto &wheel : presentation.wheels) {
      wheels << _("Wheel") << ": " << OptionalText(wheel.name) << '\n';
      AppendValue(wheels, _("Type"), OptionalText(wheel.type));
      for (const auto &slot : wheel.slots) {
        wheels << "  [" << slot.index << "] " << OptionalText(slot.name)
               << '\n';
        AppendValue(wheels, "    " + _("Color"), OptionalText(slot.rawColor));
        AppendValue(wheels, "    " + _("Filter"), OptionalText(slot.filter));
        AppendValue(wheels, "    " + _("Media reference"),
                    OptionalText(slot.mediaReference));
        AppendValue(wheels, "    " + _("Archive resource"),
                    OptionalText(slot.archiveResource));
        AppendValue(wheels, "    " + _("Graphic wheel reference"),
                    OptionalText(slot.graphicWheelReference));
        AppendValue(wheels, "    " + _("Graphic wheel resource"),
                    OptionalText(slot.graphicWheelResource));
      }
      wheels << '\n';
    }
    for (const auto &filter : presentation.filters)
      wheels << _("Filter") << ": " << OptionalText(filter.name)
             << " | " << _("Color") << ": " << OptionalText(filter.rawColor)
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
  const auto presentation = BuildGdtfInspectorModePresentation(
      modeDocument_->modes[static_cast<std::size_t>(selection)]);
  wxString text;
  AppendValue(text, _("Description"), OptionalText(presentation.description));
  AppendValue(text, _("Geometry"), OptionalText(presentation.geometry));
  AppendValue(text, _("Calculated footprint"),
              wxString::Format("%d", presentation.calculatedFootprint));
  text << '\n';
  std::unordered_map<std::string, int> depths;
  for (const auto &row : presentation.nodes) {
    const int depth = row.parentId.empty() ? 0 : depths[row.parentId] + 1;
    depths[row.id] = depth;
    text << wxString(' ', depth * 2) << "- " << Text(row.item);
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
    pages_->ChangeSelection(page);
}

} // namespace gui::inspection
