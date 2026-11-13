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
namespace viewer2d {
// Canvas stroke widths are captured device pixels, not world distances.
// Keep the established PDF appearance in paper points, independently of zoom.
inline constexpr double kViewportStrokePointsPerPixel = 0.5625;
inline double PaperStrokeScale(double worldToOutput, double paperToOutput = 1.0,
                               double pointsPerPixel = kViewportStrokePointsPerPixel) {
  return worldToOutput > 0 ? pointsPerPixel * paperToOutput / worldToOutput : 0;
}
}
