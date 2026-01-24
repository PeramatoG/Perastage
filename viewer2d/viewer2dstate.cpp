/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "viewer2dstate.h"

#include "configmanager.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"

#include <algorithm>
#include <array>
#include <unordered_set>
#include <utility>

namespace viewer2d {
namespace {
const std::array<const char *, 3> kLabelNameKeys = {"label_show_name_top",
                                                    "label_show_name_front",
                                                    "label_show_name_side"};
const std::array<const char *, 3> kLabelIdKeys = {"label_show_id_top",
                                                  "label_show_id_front",
                                                  "label_show_id_side"};
const std::array<const char *, 3> kLabelDmxKeys = {"label_show_dmx_top",
                                                   "label_show_dmx_front",
                                                   "label_show_dmx_side"};
const std::array<const char *, 3> kLabelOffsetDistanceKeys = {
    "label_offset_distance_top", "label_offset_distance_front",
    "label_offset_distance_side"};
const std::array<const char *, 3> kLabelOffsetAngleKeys = {
    "label_offset_angle_top", "label_offset_angle_front",
    "label_offset_angle_side"};
} // namespace

Viewer2DState CaptureState(const Viewer2DPanel *panel,
                           const ConfigManager &cfg) {
  Viewer2DState state;
  if (panel) {
    const auto viewState = panel->GetViewState();
    state.camera.offsetPixelsX = viewState.offsetPixelsX;
    state.camera.offsetPixelsY = viewState.offsetPixelsY;
    state.camera.zoom = viewState.zoom;
    state.camera.viewportWidth = viewState.viewportWidth;
    state.camera.viewportHeight = viewState.viewportHeight;
    state.camera.view = static_cast<int>(viewState.view);
  } else {
    state.camera.offsetPixelsX = cfg.GetFloat("view2d_offset_x");
    state.camera.offsetPixelsY = cfg.GetFloat("view2d_offset_y");
    state.camera.zoom = cfg.GetFloat("view2d_zoom");
    state.camera.view = static_cast<int>(cfg.GetFloat("view2d_view"));
  }

  state.renderOptions.renderMode =
      static_cast<int>(cfg.GetFloat("view2d_render_mode"));
  state.renderOptions.darkMode = cfg.GetFloat("view2d_dark_mode") != 0.0f;
  state.renderOptions.showGrid = cfg.GetFloat("grid_show") != 0.0f;
  state.renderOptions.gridStyle =
      static_cast<int>(cfg.GetFloat("grid_style"));
  state.renderOptions.gridColorR = cfg.GetFloat("grid_color_r");
  state.renderOptions.gridColorG = cfg.GetFloat("grid_color_g");
  state.renderOptions.gridColorB = cfg.GetFloat("grid_color_b");
  state.renderOptions.gridDrawAbove = cfg.GetFloat("grid_draw_above") != 0.0f;

  for (size_t i = 0; i < state.renderOptions.showLabelName.size(); ++i) {
    state.renderOptions.showLabelName[i] =
        cfg.GetFloat(kLabelNameKeys[i]) != 0.0f;
    state.renderOptions.showLabelId[i] =
        cfg.GetFloat(kLabelIdKeys[i]) != 0.0f;
    state.renderOptions.showLabelDmx[i] =
        cfg.GetFloat(kLabelDmxKeys[i]) != 0.0f;
    state.renderOptions.labelOffsetDistance[i] =
        cfg.GetFloat(kLabelOffsetDistanceKeys[i]);
    state.renderOptions.labelOffsetAngle[i] =
        cfg.GetFloat(kLabelOffsetAngleKeys[i]);
  }
  state.renderOptions.labelFontSizeName = cfg.GetFloat("label_font_size_name");
  state.renderOptions.labelFontSizeId = cfg.GetFloat("label_font_size_id");
  state.renderOptions.labelFontSizeDmx = cfg.GetFloat("label_font_size_dmx");

  state.layers.hiddenLayers.clear();
  auto hidden = cfg.GetHiddenLayers();
  state.layers.hiddenLayers.insert(state.layers.hiddenLayers.end(),
                                   hidden.begin(), hidden.end());
  std::sort(state.layers.hiddenLayers.begin(), state.layers.hiddenLayers.end());

  return state;
}

void ApplyState(Viewer2DPanel *panel, Viewer2DRenderPanel *renderPanel,
                ConfigManager &cfg, const Viewer2DState &state,
                bool persistCameraToConfig, bool updatePanels) {
  if (persistCameraToConfig) {
    cfg.SetFloat("view2d_offset_x", state.camera.offsetPixelsX);
    cfg.SetFloat("view2d_offset_y", state.camera.offsetPixelsY);
    cfg.SetFloat("view2d_zoom", state.camera.zoom);
    cfg.SetFloat("view2d_view", static_cast<float>(state.camera.view));
  }

  cfg.SetFloat("view2d_render_mode",
               static_cast<float>(state.renderOptions.renderMode));
  cfg.SetFloat("view2d_dark_mode", state.renderOptions.darkMode ? 1.0f : 0.0f);
  cfg.SetFloat("grid_show", state.renderOptions.showGrid ? 1.0f : 0.0f);
  cfg.SetFloat("grid_style", static_cast<float>(state.renderOptions.gridStyle));
  cfg.SetFloat("grid_color_r", state.renderOptions.gridColorR);
  cfg.SetFloat("grid_color_g", state.renderOptions.gridColorG);
  cfg.SetFloat("grid_color_b", state.renderOptions.gridColorB);
  cfg.SetFloat("grid_draw_above",
               state.renderOptions.gridDrawAbove ? 1.0f : 0.0f);

  for (size_t i = 0; i < state.renderOptions.showLabelName.size(); ++i) {
    cfg.SetFloat(kLabelNameKeys[i],
                 state.renderOptions.showLabelName[i] ? 1.0f : 0.0f);
    cfg.SetFloat(kLabelIdKeys[i],
                 state.renderOptions.showLabelId[i] ? 1.0f : 0.0f);
    cfg.SetFloat(kLabelDmxKeys[i],
                 state.renderOptions.showLabelDmx[i] ? 1.0f : 0.0f);
    cfg.SetFloat(kLabelOffsetDistanceKeys[i],
                 state.renderOptions.labelOffsetDistance[i]);
    cfg.SetFloat(kLabelOffsetAngleKeys[i],
                 state.renderOptions.labelOffsetAngle[i]);
  }
  cfg.SetFloat("label_font_size_name",
               state.renderOptions.labelFontSizeName);
  cfg.SetFloat("label_font_size_id", state.renderOptions.labelFontSizeId);
  cfg.SetFloat("label_font_size_dmx", state.renderOptions.labelFontSizeDmx);

  std::unordered_set<std::string> hidden(state.layers.hiddenLayers.begin(),
                                         state.layers.hiddenLayers.end());
  cfg.SetHiddenLayers(hidden);

  if (panel && updatePanels) {
    if (persistCameraToConfig) {
      panel->LoadViewFromConfig();
    } else {
      panel->ApplyViewState(
          state.camera.offsetPixelsX, state.camera.offsetPixelsY,
          state.camera.zoom, static_cast<Viewer2DView>(state.camera.view),
          static_cast<Viewer2DRenderMode>(state.renderOptions.renderMode));
    }
    panel->UpdateScene(true);
    panel->Refresh();
  }
  if (renderPanel && updatePanels)
    renderPanel->ApplyConfig();
}

ScopedViewer2DState::ScopedViewer2DState(Viewer2DPanel *applyPanel,
                                         Viewer2DRenderPanel *applyRenderPanel,
                                         ConfigManager &cfg,
                                         const Viewer2DState &state,
                                         Viewer2DPanel *restorePanel,
                                         Viewer2DRenderPanel
                                             *restoreRenderPanel,
                                         bool persistCameraToConfig)
    : cfg_(&cfg), applyPanel_(applyPanel),
      applyRenderPanel_(applyRenderPanel), restorePanel_(restorePanel),
      restoreRenderPanel_(restoreRenderPanel),
      restored_(false), persistCameraToConfig_(persistCameraToConfig) {
  previousState_ = CaptureState(restorePanel ? restorePanel : applyPanel, cfg);
  ApplyState(applyPanel_.get(), applyRenderPanel_.get(), cfg, state,
             persistCameraToConfig_, true);
}

ScopedViewer2DState::~ScopedViewer2DState() { Restore(); }

ScopedViewer2DState::ScopedViewer2DState(ScopedViewer2DState &&other) noexcept {
  *this = std::move(other);
}

ScopedViewer2DState &
ScopedViewer2DState::operator=(ScopedViewer2DState &&other) noexcept {
  if (this == &other)
    return *this;
  Restore();
  cfg_ = other.cfg_;
  applyPanel_ = other.applyPanel_;
  applyRenderPanel_ = other.applyRenderPanel_;
  restorePanel_ = other.restorePanel_;
  restoreRenderPanel_ = other.restoreRenderPanel_;
  previousState_ = std::move(other.previousState_);
  restored_ = other.restored_;
  persistCameraToConfig_ = other.persistCameraToConfig_;

  other.cfg_ = nullptr;
  other.applyPanel_ = wxWeakRef<Viewer2DPanel>();
  other.applyRenderPanel_ = wxWeakRef<Viewer2DRenderPanel>();
  other.restorePanel_ = wxWeakRef<Viewer2DPanel>();
  other.restoreRenderPanel_ = wxWeakRef<Viewer2DRenderPanel>();
  other.restored_ = true;
  other.persistCameraToConfig_ = true;
  return *this;
}

void ScopedViewer2DState::Restore() {
  if (!cfg_ || restored_)
    return;

  Viewer2DPanel *targetPanel =
      restorePanel_ ? restorePanel_.get() : Viewer2DPanel::Instance();
  Viewer2DRenderPanel *targetRenderPanel =
      restoreRenderPanel_ ? restoreRenderPanel_.get()
                          : Viewer2DRenderPanel::Instance();

  ApplyState(targetPanel, targetRenderPanel, *cfg_, previousState_,
             persistCameraToConfig_, true);
  restored_ = true;
}

Viewer2DState FromLayoutDefinition(const layouts::Layout2DViewDefinition &view) {
  Viewer2DState state;
  state.camera = view.camera;
  state.renderOptions = view.renderOptions;
  state.layers = view.layers;
  return state;
}

void ApplyEditorRenderOptions(Viewer2DState &state, const ConfigManager &cfg) {
  Viewer2DState editorState = CaptureState(nullptr, cfg);
  state.renderOptions = editorState.renderOptions;
  state.layers = editorState.layers;
}

layouts::Layout2DViewDefinition ToLayoutDefinition(
    const Viewer2DState &state, const layouts::Layout2DViewFrame &frame) {
  layouts::Layout2DViewDefinition view;
  view.frame = frame;
  view.camera = state.camera;
  if (frame.width > 0) {
    view.camera.viewportWidth = frame.width;
  }
  if (frame.height > 0) {
    view.camera.viewportHeight = frame.height;
  }
  view.renderOptions = state.renderOptions;
  view.layers = state.layers;
  return view;
}

layouts::Layout2DViewDefinition CaptureLayoutDefinition(
    const Viewer2DPanel *panel, const ConfigManager &cfg,
    const layouts::Layout2DViewFrame &frame) {
  layouts::Layout2DViewFrame resolvedFrame = frame;
  if (panel && (resolvedFrame.width <= 0 || resolvedFrame.height <= 0)) {
    const auto viewState = panel->GetViewState();
    resolvedFrame.width = viewState.viewportWidth;
    resolvedFrame.height = viewState.viewportHeight;
  }
  Viewer2DState state = CaptureState(panel, cfg);
  ApplyEditorRenderOptions(state, cfg);
  return ToLayoutDefinition(state, resolvedFrame);
}

} // namespace viewer2d
