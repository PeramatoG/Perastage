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
#include <optional>
#include <unordered_map>

namespace TrussDictionary {
    // Loads the dictionary file into a map of model -> path inside truss library
    std::optional<std::unordered_map<std::string, std::string>> Load();
    // Saves the dictionary map back to disk storing only filenames
    void Save(const std::unordered_map<std::string, std::string>& dict);
    // Returns stored path for a model if exists and file exists; removes missing entries
    std::optional<std::string> Get(const std::string& model);
    // Copies model file into trusses library and updates dictionary
    void Update(const std::string& model, const std::string& modelPath);
}
