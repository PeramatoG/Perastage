#include "tools/fixture_symbol_generation_tool.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <wx/msgdlg.h>

#include "guiconfigservices.h"
#include "configmanager.h"
#include "dialogs/GenerateFixtureSymbolsDialog.h"
#include "mainwindow.h"
#include "opaque_pass_utils.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpanel.h"
#include "windows/SymbolPreviewWindow.h"

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

wxImage BuildWxImageFromRgba(const std::vector<unsigned char> &pixels, int width,
                             int height) {
  if (width <= 0 || height <= 0)
    return wxImage();

  const size_t pixelCount = static_cast<size_t>(width) *
                            static_cast<size_t>(height);
  const size_t expectedBytes = pixelCount * 4;
  if (pixels.size() < expectedBytes)
    return wxImage();

  unsigned char *rgb = new unsigned char[pixelCount * 3];
  unsigned char *alpha = new unsigned char[pixelCount];
  for (size_t i = 0; i < pixelCount; ++i) {
    rgb[i * 3 + 0] = pixels[i * 4 + 0];
    rgb[i * 3 + 1] = pixels[i * 4 + 1];
    rgb[i * 3 + 2] = pixels[i * 4 + 2];
    alpha[i] = pixels[i * 4 + 3];
  }

  wxImage image(width, height);
  image.SetData(rgb, true);
  image.SetAlpha(alpha, true);
  return image;
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

  Viewer2DOffscreenRenderer *offscreenRenderer = window.GetOffscreenRenderer();
  if (!offscreenRenderer) {
    wxMessageBox("Could not prepare offscreen renderer.",
                 "Generate Fixture Symbols", wxOK | wxICON_ERROR, &window);
    return;
  }

  CaptureRequiredViews(capturePanel);
  const Viewer2DView previousView = capturePanel->GetView();
  capturePanel->SetView(Viewer2DView::Top);

  offscreenRenderer->SetViewportSize(wxSize(1200, 1200));
  offscreenRenderer->PrepareForCapture();

  std::vector<unsigned char> pixels;
  int width = 0;
  int height = 0;
  const bool ok = capturePanel->RenderToRGBA(pixels, width, height);
  capturePanel->SetView(previousView);
  if (!ok || width <= 0 || height <= 0) {
    wxMessageBox("Could not capture source image from 2D viewer.",
                 "Generate Fixture Symbols", wxOK | wxICON_ERROR, &window);
    return;
  }

  wxImage image = BuildWxImageFromRgba(pixels, width, height);
  if (!image.IsOk()) {
    wxMessageBox("Captured image is not valid.", "Generate Fixture Symbols",
                 wxOK | wxICON_ERROR, &window);
    return;
  }

  SymbolPreviewWindow *preview = new SymbolPreviewWindow(&window, image);
  preview->Show();
}

} // namespace tools
