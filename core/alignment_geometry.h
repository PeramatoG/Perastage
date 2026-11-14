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
#include <array>
#include <string>
#include <vector>

namespace alignment {
enum class Reference { Extents, First, Last };
enum class Operation { Top, Bottom, Left, Right, CenterHorizontal, CenterVertical };
enum class Plane { XY, XZ, YZ };
struct Projection { int horizontal; int vertical; int horizontalSign; int verticalSign; };
// Side is viewed along +X: screen right is negative world Y.
Projection WorkPlane(Plane plane);
struct Rectangle { double left, bottom, right, top; };
struct Bounds { std::array<double, 3> min, max; };
struct Plan {
  bool valid = false;
  bool changed = false;
  std::string diagnostic;
  std::vector<double> offsets;
};
// Pure document/scene-independent arithmetic. Input order defines first/last.
Plan Calculate(const std::vector<Rectangle> &rectangles, Reference reference,
               Operation operation, double tolerance = 0.0001);
bool Valid(const Bounds &bounds);
Rectangle Project(const Bounds &bounds, Plane plane);
std::array<double, 3> WorldDelta(double offset, Plane plane, Operation operation);
} // namespace alignment
