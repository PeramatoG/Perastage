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
#pragma once

#include "layouts/LayoutCollection.h"

#include <wx/weakref.h>

class ConfigManager;
class Viewer2DPanel;
class Viewer2DRenderPanel;

namespace viewer2d {

struct Viewer2DState {
  layouts::Layout2DViewCameraState camera;
  layouts::Layout2DViewRenderOptions renderOptions;
  layouts::Layout2DViewLayers layers;
};

Viewer2DState CaptureState(const Viewer2DPanel *panel,
                           const ConfigManager &cfg);
void ApplyState(Viewer2DPanel *panel, Viewer2DRenderPanel *renderPanel,
                ConfigManager &cfg, const Viewer2DState &state);

class ScopedViewer2DState {
public:
  ScopedViewer2DState(Viewer2DPanel *applyPanel,
                      Viewer2DRenderPanel *applyRenderPanel,
                      ConfigManager &cfg, const Viewer2DState &state,
                      Viewer2DPanel *restorePanel = nullptr,
                      Viewer2DRenderPanel *restoreRenderPanel = nullptr);
  ~ScopedViewer2DState();

  ScopedViewer2DState(const ScopedViewer2DState &) = delete;
  ScopedViewer2DState &operator=(const ScopedViewer2DState &) = delete;
  ScopedViewer2DState(ScopedViewer2DState &&other) noexcept;
  ScopedViewer2DState &operator=(ScopedViewer2DState &&other) noexcept;

  void Restore();
  bool IsActive() const { return cfg_ != nullptr && !restored_; }

private:
  ConfigManager *cfg_ = nullptr;
  wxWeakRef<Viewer2DPanel> applyPanel_;
  wxWeakRef<Viewer2DRenderPanel> applyRenderPanel_;
  wxWeakRef<Viewer2DPanel> restorePanel_;
  wxWeakRef<Viewer2DRenderPanel> restoreRenderPanel_;
  Viewer2DState previousState_;
  bool restored_ = true;
};

Viewer2DState FromLayoutDefinition(const layouts::Layout2DViewDefinition &view);
layouts::Layout2DViewDefinition ToLayoutDefinition(
    const Viewer2DState &state, const layouts::Layout2DViewFrame &frame = {});
layouts::Layout2DViewDefinition CaptureLayoutDefinition(
    const Viewer2DPanel *panel, const ConfigManager &cfg,
    const layouts::Layout2DViewFrame &frame = {});

} // namespace viewer2d
