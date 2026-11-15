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
#include "viewer2dpanel.h"
#include "configmanager.h"
#include "ui_render_size.h"
#include "pixel_coordinate_math.h"

std::optional<std::string> Viewer2DPanel::PickFixtureForLayoutLabels(const wxPoint &position) {
  const auto size = ResolveRenderSize(this);
  if (!size.IsValid() || !IsShownOnScreen() || !TryBindGlContextForInteraction()) return {};
  const auto point = pixel_coordinates::LogicalToFramebuffer(
      {double(position.x), double(position.y)}, GetContentScaleFactor());
  if (!point) return {};
  auto &cfg = ConfigManager::Get();
  std::string uuid;
  if (!m_controller.GetPickUuidAt((*point)[0], (*point)[1], size.width, size.height,
                                  cfg.GetHiddenLayers(), uuid) ||
      !cfg.GetScene().fixtures.contains(uuid)) return {};
  return uuid;
}

void Viewer2DPanel::SetLayoutFixtureLabelSelection(std::vector<std::string> selection) {
  layoutFixtureLabelSelection = std::move(selection);
  m_controller.SetSelectionOutlineEnabled(true);
  m_controller.SetSelectedUuids(*layoutFixtureLabelSelection);
  RequestRepaint();
}
