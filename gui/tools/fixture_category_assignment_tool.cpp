#include "tools/fixture_category_assignment_tool.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <wx/choicdlg.h>
#include <wx/msgdlg.h>

#include "configmanager.h"
#include "fixture.h"
#include "gdtf_fixture_category.h"
#include "gdtfdictionary.h"
#include "guiconfigservices.h"
#include "mainwindow.h"

namespace tools {
namespace {

const std::vector<std::string> &CategoryChoices() {
  static const std::vector<std::string> kChoices = {
      GdtfFixtureCategory::kUnknown,      GdtfFixtureCategory::kHoist,
      GdtfFixtureCategory::kSmoke,        GdtfFixtureCategory::kLaser,
      GdtfFixtureCategory::kVideo,        GdtfFixtureCategory::kFx,
      GdtfFixtureCategory::kStrobe,       GdtfFixtureCategory::kLed,
      GdtfFixtureCategory::kBlinder,      GdtfFixtureCategory::kConventional,
      GdtfFixtureCategory::kWash,         GdtfFixtureCategory::kSpot,
      GdtfFixtureCategory::kBeam,         GdtfFixtureCategory::kHybrid};
  return kChoices;
}

} // namespace

void RunFixtureCategoryAssignment(MainWindow &window) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const std::vector<std::string> selectedUuids = cfg.GetSelectedFixtures();
  if (selectedUuids.empty()) {
    wxMessageBox("Select at least one fixture first.",
                 "Assign fixture category", wxOK | wxICON_INFORMATION, &window);
    return;
  }

  wxArrayString choices;
  for (const auto &category : CategoryChoices())
    choices.Add(wxString::FromUTF8(category));

  wxSingleChoiceDialog dialog(&window,
                              "Select the category to assign to all selected fixtures.",
                              "Assign fixture category", choices);
  if (dialog.ShowModal() != wxID_OK)
    return;

  const std::string selectedCategory = GdtfFixtureCategory::NormalizeCategory(
      std::string(dialog.GetStringSelection().ToUTF8()));
  if (selectedCategory.empty())
    return;

  auto &scene = cfg.GetScene();
  bool changed = false;
  bool undoPushed = false;
  std::set<std::string> updatedTypes;
  std::size_t updatedCount = 0;
  for (const std::string &uuid : selectedUuids) {
    auto it = scene.fixtures.find(uuid);
    if (it == scene.fixtures.end())
      continue;

    Fixture &fixture = it->second;
    if (fixture.category == selectedCategory &&
        fixture.categorySource == GdtfFixtureCategory::kManualSource) {
      continue;
    }

    if (!undoPushed) {
      cfg.PushUndoState("assign fixture category");
      undoPushed = true;
    }
    fixture.category = selectedCategory;
    fixture.categorySource = GdtfFixtureCategory::kManualSource;
    if (!fixture.typeName.empty())
      updatedTypes.insert(fixture.typeName);
    ++updatedCount;
    changed = true;
  }

  if (!changed) {
    wxMessageBox("Selected fixtures already have this category.",
                 "Assign fixture category", wxOK | wxICON_INFORMATION, &window);
    return;
  }

  for (const auto &typeName : updatedTypes)
    GdtfDictionary::UpdateCategory(typeName, selectedCategory);

  window.RefreshAfterToolSceneUpdate();

  wxMessageBox(
      wxString::Format("Assigned category '%s' to %zu fixture(s).",
                       wxString::FromUTF8(selectedCategory), updatedCount),
      "Assign fixture category", wxOK | wxICON_INFORMATION, &window);
}

} // namespace tools
