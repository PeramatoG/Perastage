#include "tools/fixture_category_assignment_tool.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <wx/msgdlg.h>

#include "configmanager.h"
#include "fixture.h"
#include "gdtf_fixture_category.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "guiconfigservices.h"
#include "mainwindow.h"

namespace tools {

namespace {

std::string ToLowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool ContainsToken(const std::string &text, const char *needle) {
  return text.find(needle) != std::string::npos;
}

std::string ResolveFixtureGdtfPath(const std::string &basePath,
                                   const std::string &gdtfSpec) {
  if (gdtfSpec.empty())
    return {};

  const std::filesystem::path specPath = std::filesystem::u8path(gdtfSpec);
  if (specPath.is_absolute() && std::filesystem::exists(specPath))
    return specPath.string();

  if (!basePath.empty()) {
    const std::filesystem::path joined = std::filesystem::u8path(basePath) / specPath;
    if (std::filesystem::exists(joined))
      return joined.string();
  }

  if (std::filesystem::exists(specPath))
    return specPath.string();

  return {};
}

bool LooksLikeWashFromChannels(const std::string &gdtfPath,
                               const std::string &modeName) {
  const auto evaluateMode = [&](const std::string &mode) {
    const std::vector<GdtfChannelInfo> channels = GetGdtfModeChannels(gdtfPath, mode);
    bool hasPan = false;
    bool hasTilt = false;
    bool hasGobo = false;
    for (const GdtfChannelInfo &channel : channels) {
      const std::string functionLower = ToLowerCopy(channel.function);
      if (ContainsToken(functionLower, "pan"))
        hasPan = true;
      if (ContainsToken(functionLower, "tilt"))
        hasTilt = true;
      if (ContainsToken(functionLower, "gobo"))
        hasGobo = true;
    }
    return hasPan && hasTilt && !hasGobo;
  };

  if (!modeName.empty())
    return evaluateMode(modeName);

  for (const std::string &mode : GetGdtfModes(gdtfPath)) {
    if (evaluateMode(mode))
      return true;
  }
  return false;
}

} // namespace

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

    const std::string resolvedGdtfPath =
        ResolveFixtureGdtfPath(scene.basePath, fixture.gdtfSpec);
    if (inferred.category == GdtfFixtureCategory::kUnknown &&
        !resolvedGdtfPath.empty()) {
      inferred = GdtfFixtureCategory::InferFromGdtf(resolvedGdtfPath);
    }

    if (inferred.category == GdtfFixtureCategory::kUnknown &&
        !resolvedGdtfPath.empty() &&
        LooksLikeWashFromChannels(resolvedGdtfPath, fixture.gdtfMode)) {
      inferred = {GdtfFixtureCategory::kWash,
                  "channel hints: pan+tilt without gobo"};
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
