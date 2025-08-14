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

#include <string>
#include <array>

// Transformation matrix used in MVR for spatial positioning
struct Matrix {
    std::array<float, 3> u{ 1.0f, 0.0f, 0.0f };
    std::array<float, 3> v{ 0.0f, 1.0f, 0.0f };
    std::array<float, 3> w{ 0.0f, 0.0f, 1.0f };
    std::array<float, 3> o{ 0.0f, 0.0f, 0.0f };
};
