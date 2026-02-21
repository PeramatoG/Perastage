#include "tools/fixture_symbol_generation_tool.h"

#include <map>
#include <array>
#include <set>
#include <string>
#include <unordered_map>
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

class ScopedFloatConfigOverride {
public:
  ScopedFloatConfigOverride(ConfigManager &cfg,
                            std::initializer_list<std::pair<const char *, float>> values)
      : cfg_(cfg) {
    for (const auto &entry : values) {
      const std::string key(entry.first);
      previous_[key] = cfg_.GetFloat(key);
      cfg_.SetFloat(key, entry.second);
    }
  }

  ~ScopedFloatConfigOverride() {
    for (const auto &[key, value] : previous_)
      cfg_.SetFloat(key, value);
  }

private:
  ConfigManager &cfg_;
  std::unordered_map<std::string, float> previous_;
};

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

  Viewer2DOffscreenRenderer *offscreenRenderer = window.GetOffscreenRenderer();
  if (!offscreenRenderer) {
    wxMessageBox("Could not prepare offscreen renderer.",
                 "Generate Fixture Symbols", wxOK | wxICON_ERROR, &window);
    return;
  }
  Viewer2DPanel *capturePanel = offscreenRenderer->GetPanel();
  if (!capturePanel) {
    wxMessageBox("Could not create 2D capture panel instance.",
                 "Generate Fixture Symbols", wxOK | wxICON_ERROR, &window);
    return;
  }

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  ScopedFloatConfigOverride displayOverride(
      cfg,
      {
          {"grid_show", 0.0f},
          {"view2d_dark_mode", 0.0f},
          {"label_show_name_top", 0.0f},
          {"label_show_name_front", 0.0f},
          {"label_show_name_side", 0.0f},
          {"label_show_id_top", 0.0f},
          {"label_show_id_front", 0.0f},
          {"label_show_id_side", 0.0f},
          {"label_show_dmx_top", 0.0f},
          {"label_show_dmx_front", 0.0f},
          {"label_show_dmx_side", 0.0f},
      });

  capturePanel->SetRenderMode(Viewer2DRenderMode::White);
  CaptureRequiredViews(capturePanel);
  const Viewer2DView previousView = capturePanel->GetView();

  offscreenRenderer->SetViewportSize(wxSize(1200, 1200));

  std::array<wxImage, 4> images;
  const std::array<Viewer2DView, 4> views = {Viewer2DView::Front,
                                             Viewer2DView::Top,
                                             Viewer2DView::Side,
                                             Viewer2DView::Bottom};
  bool allOk = true;
  for (size_t i = 0; i < views.size(); ++i) {
    capturePanel->SetView(views[i]);
    capturePanel->UpdateScene(true);
    offscreenRenderer->PrepareForCapture();

    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    const bool ok = capturePanel->RenderToRGBA(pixels, width, height);
    if (!ok || width <= 0 || height <= 0) {
      allOk = false;
      break;
    }

    images[i] = BuildWxImageFromRgba(pixels, width, height);
    if (!images[i].IsOk()) {
      allOk = false;
      break;
    }
  }
  capturePanel->SetView(previousView);

  if (!allOk) {
    wxMessageBox("Could not capture all orthographic source images from the 2D viewer.",
                 "Generate Fixture Symbols", wxOK | wxICON_ERROR, &window);
    return;
  }

  SymbolPreviewWindow *preview = new SymbolPreviewWindow(&window, images);
  preview->Show();
}

} // namespace tools
