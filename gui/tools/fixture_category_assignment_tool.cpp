#include "tools/fixture_category_assignment_tool.h"

#include <filesystem>
#include <set>
#include <string>

#include <wx/msgdlg.h>

#include "configmanager.h"
#include "fixture.h"
#include "gdtf_fixture_category.h"
#include "gdtfdictionary.h"
#include "guiconfigservices.h"
#include "mainwindow.h"

namespace tools {

void RunFixtureCategoryAssignment(MainWindow &window) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const std::vector<std::string> selectedUuids = cfg.GetSelectedFixtures();
  if (selectedUuids.empty()) {
    wxMessageBox("Select at least one fixture first.",
                 "Auto-assign fixture categories", wxOK | wxICON_INFORMATION,
                 &window);
    return;
  }

  auto &scene = cfg.GetScene();
  bool changed = false;
  bool undoPushed = false;
  std::set<std::pair<std::string, std::string>> updatedTypeCategories;
  std::size_t updatedCount = 0;
  std::size_t unknownCount = 0;
  for (const std::string &uuid : selectedUuids) {
    auto it = scene.fixtures.find(uuid);
    if (it == scene.fixtures.end())
      continue;

    Fixture &fixture = it->second;
    const std::string preferredName =
        fixture.typeName.empty() ? fixture.instanceName : fixture.typeName;
    auto inferred = GdtfFixtureCategory::InferFromName(preferredName);

    if (inferred.category == GdtfFixtureCategory::kUnknown &&
        !fixture.gdtfSpec.empty() &&
        std::filesystem::exists(fixture.gdtfSpec)) {
      inferred = GdtfFixtureCategory::InferFromGdtf(fixture.gdtfSpec);
    }

    std::string inferredCategory =
        GdtfFixtureCategory::NormalizeCategory(inferred.category);
    if (inferredCategory.empty())
      inferredCategory = GdtfFixtureCategory::kUnknown;
    if (inferredCategory == GdtfFixtureCategory::kUnknown)
      ++unknownCount;

    if (fixture.category == inferredCategory &&
        fixture.categorySource == GdtfFixtureCategory::kAutoFallbackSource) {
      continue;
    }

    if (!undoPushed) {
      cfg.PushUndoState("auto assign fixture categories");
      undoPushed = true;
    }
    fixture.category = inferredCategory;
    fixture.categorySource = GdtfFixtureCategory::kAutoFallbackSource;
    fixture.categorySourceReason = inferred.reason;
    if (!fixture.typeName.empty() &&
        inferredCategory != GdtfFixtureCategory::kUnknown) {
      updatedTypeCategories.insert({fixture.typeName, inferredCategory});
    }
    ++updatedCount;
    changed = true;
  }

  if (!changed) {
    wxMessageBox("Selected fixtures are already up to date.",
                 "Auto-assign fixture categories", wxOK | wxICON_INFORMATION,
                 &window);
    return;
  }

  (void)updatedTypeCategories;

  window.RefreshAfterToolSceneUpdate();

  wxMessageBox(wxString::Format(
                   "Auto-assigned categories to %zu fixture(s). Unknown: %zu.",
                   updatedCount, unknownCount),
               "Auto-assign fixture categories", wxOK | wxICON_INFORMATION,
               &window);
}

} // namespace tools
