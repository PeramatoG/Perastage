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
#include <string>
#include <vector>
#include "../models/types.h"
#include "../viewer3d/mesh.h"

struct GdtfObject { Mesh mesh; Matrix transform; };

bool LoadGdtf(const std::string&, std::vector<GdtfObject>&, std::string*) {
    return false;
}
int GetGdtfModeChannelCount(const std::string&, const std::string&) { return 0; }
std::string GetGdtfFixtureName(const std::string& gdtfPath) { return gdtfPath; }
std::string GetGdtfModelColor(const std::string& gdtfPath) { return "#000000"; }
bool SetGdtfModelColor(const std::string&, const std::string&) { return true; }
bool GetGdtfProperties(const std::string&, float &w, float &p) { w = 0.0f; p = 0.0f; return false; }
