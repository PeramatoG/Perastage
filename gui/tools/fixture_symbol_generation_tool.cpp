#include "tools/fixture_symbol_generation_tool.h"

#include <map>
#include <set>
#include <string>

#include <wx/msgdlg.h>

#include "configservices.h"
#include "dialogs/GenerateFixtureSymbolsDialog.h"
#include "mainwindow.h"
#include "opaque_pass_utils.h"
#include "viewer2dpanel.h"
#include "windows/SymbolPreviewWindow.h"
#include "symbols/Symbol2DBuilder.h"

namespace tools {
namespace {

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

void CaptureRequiredViews(Viewer2DPanel *panel) {
  if (!panel)
    return;
  const Viewer2DView previous = panel->GetView();
  for (Viewer2DView view : {Viewer2DView::Front, Viewer2DView::Top,
                            Viewer2DView::Bottom, Viewer2DView::Side}) {
    panel->SetView(view);
    panel->CaptureFrameNow([](CommandBuffer, Viewer2DViewState) {}, true, false);
  }
  panel->SetView(previous);
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

  Viewer2DPanel *capturePanel = window.GetLayoutCapturePanel();
  if (!capturePanel) {
    wxMessageBox("2D viewer is not available for symbol generation.",
                 "Generate Fixture Symbols", wxOK | wxICON_WARNING, &window);
    return;
  }

  CaptureRequiredViews(capturePanel);
  auto snapshot = capturePanel->GetBottomSymbolCacheSnapshot();
  if (!snapshot) {
    wxMessageBox("Could not capture symbol data.", "Generate Fixture Symbols",
                 wxOK | wxICON_ERROR, &window);
    return;
  }

  auto symbols = symbols::Symbol2DBuilder::BuildForFixtureModelKeys(
      options[selection].modelKeys, *snapshot);
  SymbolPreviewWindow *preview = new SymbolPreviewWindow(&window, std::move(symbols));
  preview->Show();
}

} // namespace tools
