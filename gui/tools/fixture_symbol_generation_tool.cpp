#include "tools/fixture_symbol_generation_tool.h"

#include <algorithm>
#include <array>
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
#include "models/sceneobject.h"
#include "models/support.h"
#include "models/truss.h"
#include "opaque_pass_utils.h"
#include "symbols/Symbol2DImageBuilder.h"
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

class ScopedFixtureColorOverride {
public:
  ScopedFixtureColorOverride(ConfigManager &cfg,
                             const std::vector<std::string> &modelKeys,
                             const std::string &forcedHex)
      : cfg_(cfg) {
    auto &fixtures = cfg_.GetScene().fixtures;
    for (auto &[uuid, fixture] : fixtures) {
      if (!FixtureMatchesModelKeys(fixture, modelKeys))
        continue;
      previous_.emplace_back(uuid, fixture.color);
      fixture.color = forcedHex;
    }
  }

  ~ScopedFixtureColorOverride() {
    auto &fixtures = cfg_.GetScene().fixtures;
    for (const auto &[uuid, color] : previous_) {
      auto it = fixtures.find(uuid);
      if (it != fixtures.end())
        it->second.color = color;
    }
  }

private:
  ConfigManager &cfg_;
  std::vector<std::pair<std::string, std::string>> previous_;
};

class ScopedSymbolCaptureSceneOverride {
public:
  ScopedSymbolCaptureSceneOverride(ConfigManager &cfg,
                                   const std::vector<std::string> &modelKeys)
      : cfg_(cfg) {
    auto &scene = cfg_.GetScene();
    originalFixtures_ = scene.fixtures;
    originalTrusses_ = scene.trusses;
    originalSceneObjects_ = scene.sceneObjects;
    originalSupports_ = scene.supports;

    std::unordered_map<std::string, Fixture> filteredFixtures;
    for (const auto &[uuid, fixture] : scene.fixtures) {
      if (FixtureMatchesModelKeys(fixture, modelKeys))
        filteredFixtures.emplace(uuid, fixture);
    }

    scene.fixtures = std::move(filteredFixtures);
    scene.trusses.clear();
    scene.sceneObjects.clear();
    scene.supports.clear();
  }

  ~ScopedSymbolCaptureSceneOverride() {
    auto &scene = cfg_.GetScene();
    scene.fixtures = std::move(originalFixtures_);
    scene.trusses = std::move(originalTrusses_);
    scene.sceneObjects = std::move(originalSceneObjects_);
    scene.supports = std::move(originalSupports_);
  }

private:
  ConfigManager &cfg_;
  std::unordered_map<std::string, Fixture> originalFixtures_;
  std::unordered_map<std::string, Truss> originalTrusses_;
  std::unordered_map<std::string, SceneObject> originalSceneObjects_;
  std::unordered_map<std::string, Support> originalSupports_;
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

void MirrorImageHorizontally(symbols::RenderedSymbolImage &render) {
  if (render.width <= 0 || render.height <= 0)
    return;
  for (int y = 0; y < render.height; ++y) {
    for (int x = 0; x < render.width / 2; ++x) {
      const int opposite = render.width - 1 - x;
      const size_t left =
          (static_cast<size_t>(y) * static_cast<size_t>(render.width) +
           static_cast<size_t>(x)) *
          4;
      const size_t right =
          (static_cast<size_t>(y) * static_cast<size_t>(render.width) +
           static_cast<size_t>(opposite)) *
          4;
      for (size_t c = 0; c < 4; ++c)
        std::swap(render.rgba[left + c], render.rgba[right + c]);
    }
  }
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
  const std::string forcedFixtureColor = "#3FA9F5";
  ScopedSymbolCaptureSceneOverride isolatedSceneOverride(
      cfg, options[static_cast<size_t>(selection)].modelKeys);
  ScopedFixtureColorOverride selectedFixtureColorOverride(
      cfg, options[static_cast<size_t>(selection)].modelKeys, forcedFixtureColor);
  ScopedFloatConfigOverride displayOverride(
      cfg,
      {
          {"grid_show", 0.0f},
          {"view2d_dark_mode", 0.0f},
          {"view2d_render_mode",
           static_cast<float>(Viewer2DRenderMode::ByFixtureType)},
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

  const Viewer2DView previousView = capturePanel->GetView();

  offscreenRenderer->SetViewportSize(wxSize(1200, 1200));
  offscreenRenderer->PrepareForCapture();
  capturePanel->SetRenderMode(Viewer2DRenderMode::ByFixtureType);
  capturePanel->UpdateScene(true);
  {
    std::vector<unsigned char> warmupPixels;
    int warmupWidth = 0;
    int warmupHeight = 0;
    (void)capturePanel->RenderToRGBA(warmupPixels, warmupWidth, warmupHeight);
  }

  struct CaptureRequest {
    Viewer2DView view = Viewer2DView::Top;
    float topFixturesInverted = 0.0f;
    bool mirrorSideHorizontally = false;
    symbols::SymbolView symbolView = symbols::SymbolView::Top;
  };
  const std::array<CaptureRequest, 4> requests = {
      CaptureRequest{Viewer2DView::Front, 0.0f, false, symbols::SymbolView::Front},
      CaptureRequest{Viewer2DView::Top, 0.0f, false, symbols::SymbolView::Top},
      CaptureRequest{Viewer2DView::Side, 0.0f, true, symbols::SymbolView::Left},
      CaptureRequest{Viewer2DView::Top, 1.0f, false, symbols::SymbolView::Bottom},
  };

  std::vector<symbols::RenderedSymbolImage> renders;
  renders.reserve(requests.size());
  bool allOk = true;
  for (const auto &request : requests) {
    ScopedFloatConfigOverride topViewOverride(
        cfg, {{"view2d_top_fixtures_inverted", request.topFixturesInverted}});

    capturePanel->SetView(request.view);
    capturePanel->UpdateScene(true);
    capturePanel->FitViewToScene();

    symbols::RenderedSymbolImage render;
    render.view = request.symbolView;
    bool ok = capturePanel->RenderToRGBA(render.rgba, render.width, render.height);
    if (!ok || render.width <= 0 || render.height <= 0) {
      allOk = false;
      break;
    }
    if (request.mirrorSideHorizontally)
      MirrorImageHorizontally(render);
    renders.push_back(std::move(render));
  }
  capturePanel->SetView(previousView);

  if (!allOk) {
    wxMessageBox("Could not capture all orthographic source images from the 2D viewer.",
                 "Generate Fixture Symbols", wxOK | wxICON_ERROR, &window);
    return;
  }

  auto symbols = symbols::Symbol2DImageBuilder::BuildFromRenderedImages(renders);
  if (symbols.size() != requests.size()) {
    wxMessageBox("Could not generate all fixture symbols from captured views.",
                 "Generate Fixture Symbols", wxOK | wxICON_ERROR, &window);
    return;
  }

  SymbolPreviewWindow *preview = new SymbolPreviewWindow(&window, std::move(symbols));
  preview->Show();
}

} // namespace tools
