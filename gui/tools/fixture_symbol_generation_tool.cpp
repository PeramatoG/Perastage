#include "tools/fixture_symbol_generation_tool.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <wx/msgdlg.h>

#include "configmanager.h"
#include "dialogs/GenerateFixtureSymbolsDialog.h"
#include "guiconfigservices.h"
#include "mainwindow.h"
#include "opaque_pass_utils.h"
#include "tools/scene_model_symbol_capture_service.h"
#include "tools/symbol_physical_calibration.h"
#include "viewer2doffscreenrenderer.h"
#include "windows/SymbolPreviewWindow.h"

namespace tools {
namespace {

bool FixtureMatchesModelKeys(const Fixture &fixture,
                             const std::vector<std::string> &modelKeys) {
  if (!fixture.gdtfSpec.empty() &&
      std::find(modelKeys.begin(), modelKeys.end(),
                NormalizeModelKey(fixture.gdtfSpec)) != modelKeys.end()) {
    return true;
  }
  if (!fixture.typeName.empty() &&
      std::find(modelKeys.begin(), modelKeys.end(), fixture.typeName) !=
          modelKeys.end()) {
    return true;
  }
  return false;
}

std::string FindSingleFixtureUuidForModelKeys(
    const std::unordered_map<std::string, Fixture> &fixtures,
    const std::vector<std::string> &modelKeys) {
  std::string selectedUuid;
  for (const auto &[uuid, fixture] : fixtures) {
    if (!FixtureMatchesModelKeys(fixture, modelKeys))
      continue;
    if (selectedUuid.empty() || uuid < selectedUuid)
      selectedUuid = uuid;
  }
  return selectedUuid;
}

std::vector<FixtureSymbolTypeOption> BuildFixtureOptions() {
  std::vector<FixtureSymbolTypeOption> options;
  auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();

  struct Aggregate {
    int count = 0;
    std::set<std::string> modelKeys;
  };
  std::map<std::string, Aggregate> byLabel;

  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    std::string label = fixture.typeName.empty() ? "Unnamed fixture" : fixture.typeName;
    Aggregate &entry = byLabel[label];
    ++entry.count;

    if (!fixture.gdtfSpec.empty())
      entry.modelKeys.insert(NormalizeModelKey(fixture.gdtfSpec));
    if (!fixture.typeName.empty())
      entry.modelKeys.insert(fixture.typeName);
  }

  for (const auto &[label, aggregate] : byLabel) {
    FixtureSymbolTypeOption option;
    option.label = label + " (" + std::to_string(aggregate.count) + ")";
    option.modelKeys.assign(aggregate.modelKeys.begin(), aggregate.modelKeys.end());
    options.push_back(std::move(option));
  }
  return options;
}

} // namespace

void RunFixtureSymbolGeneration(MainWindow &window) {
  auto options = BuildFixtureOptions();
  if (options.empty()) {
    wxMessageBox("No fixtures available in this project.", "Generate Fixture Symbols",
                 wxOK | wxICON_INFORMATION, &window);
    return;
  }

  GenerateFixtureSymbolsDialog dialog(&window, options);
  if (dialog.ShowModal() != wxID_OK)
    return;

  const int selection = dialog.GetSelectionIndex();
  if (selection == wxNOT_FOUND || selection < 0 ||
      selection >= static_cast<int>(options.size())) {
    return;
  }
  Viewer2DOffscreenRenderer *offscreenRenderer = window.GetOffscreenRenderer();
  if (!offscreenRenderer) {
    wxMessageBox("Could not prepare offscreen renderer.",
                 "Generate Fixture Symbols", wxOK | wxICON_ERROR, &window);
    return;
  }
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const std::string selectedFixtureUuid =
      FindSingleFixtureUuidForModelKeys(cfg.GetScene().fixtures,
                                        options[static_cast<size_t>(selection)].modelKeys);
  if (selectedFixtureUuid.empty()) {
    wxMessageBox("Could not isolate a fixture instance for this fixture type.",
                 "Generate Fixture Symbols", wxOK | wxICON_ERROR, &window);
    return;
  }

  const std::string forcedFixtureColor = "#3FA9F5";
  SceneModelSymbolCaptureOptions captureOptions;
  captureOptions.forcedFixtureColor = forcedFixtureColor;
  auto capture = CaptureSceneModelOrthographicSymbols(
      *offscreenRenderer, cfg,
      SceneModelSymbolTarget{SceneModelKind::Fixture, selectedFixtureUuid},
      captureOptions);
  if (!capture.ok) {
    wxMessageBox(capture.error, "Generate Fixture Symbols", wxOK | wxICON_ERROR,
                 &window);
    return;
  }

  std::string calibrationError;
  if (!CalibrateFixtureSymbolsToPhysicalUnits(cfg, selectedFixtureUuid,
                                              capture.symbols,
                                              calibrationError)) {
    wxMessageBox(calibrationError, "Generate Fixture Symbols", wxOK | wxICON_ERROR,
                 &window);
    return;
  }

  SymbolPreviewWindow *preview = new SymbolPreviewWindow(
      &window, std::move(capture.symbols), selectedFixtureUuid);
  preview->Show();
}

} // namespace tools
