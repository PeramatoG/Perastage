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
#include "viewer2dpdfexporter.h"
#include "viewer2dcommandrenderer.h"
#include <cmath>
#include <string>
namespace viewer2d::pdf {
// Validates the viewport before composing it into the existing document page.
inline std::string BuildLayoutViewMapping(const LayoutViewExportData &view,
    const Viewer2DPrintOptions &options, Viewer2DRenderMapping &mapping) {
  if (view.buffer.commands.empty()) return "Unable to capture one or more layout views.";
  if (view.viewState.viewportWidth <= 0 || view.viewState.viewportHeight <= 0)
    return "The 2D viewport is not ready for layout export.";
  if (!std::isfinite(view.viewState.zoom) || view.viewState.zoom <= 0)
    return "Invalid zoom value provided for layout export.";
  const auto &f = view.frame;
  if (!std::isfinite(f.x) || !std::isfinite(f.y) || !std::isfinite(f.width) ||
      !std::isfinite(f.height) || f.width <= 0 || f.height <= 0)
    return "Layout frame dimensions are invalid for export.";
  try {
    layouts::ValidatePhysicalOutput(view.viewState.physicalMapping.has_value(),
        options.layoutScaleX, options.layoutScaleY);
    if (view.viewState.physicalMapping) layouts::ValidateViewportFrame(f);
  } catch (const std::invalid_argument &e) { return e.what(); }
  if (!BuildViewMapping(view.viewState, f.width, f.height, 0.0, mapping))
    return "Layout view dimensions are invalid for export.";
  return {};
}
} // namespace viewer2d::pdf
