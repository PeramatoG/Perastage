/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
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
#include "gdtf_resolution.h"
#include "../mvr/mvr_scene_node_reader.h"
#include <filesystem>
#include <functional>
#include <unordered_map>
class wxWindow;

std::unordered_map<std::string, gdtf_resolution::Resolution>
RunMvrGdtfResolution(wxWindow *parent,
    const std::vector<mvr::SceneReadGdtfConflict> &requests,
    const std::filesystem::path &sceneRoot,
    std::function<void(const std::string &)> log,
    const std::unordered_map<std::string, std::vector<std::string>> &importedModes = {});
