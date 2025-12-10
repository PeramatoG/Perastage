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

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include "types.h"

// Represents a hoist parsed from MVR
struct Support {
    std::string uuid;
    std::string name;
    std::string gdtfSpec;
    std::string gdtfMode;
    std::string function;
    float chainLength = 0.0f;
    std::string position;
    std::string positionName;
    std::string layer;

    float capacityKg = 0.0f;
    float weightKg = 0.0f;
    // Hoist function/category. Defaults to values returned by GetHoistFunctionOptions().
    std::string hoistFunction = "Lighting";

    Matrix transform;
};

inline const std::array<std::string, 5> &GetHoistFunctionOptions() {
    static const std::array<std::string, 5> options = {
        "Lighting", "Audio", "Video", "Scenic", "Extra"};
    return options;
}

inline std::string NormalizeHoistFunction(const std::string &rawValue) {
    auto trimmed = rawValue;
    auto trimSpaces = [](std::string &s) {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    };
    trimSpaces(trimmed);

    if (trimmed.empty())
        return "Lighting";

    try {
        if (std::stod(trimmed) == 0.0)
            return "Lighting";
    } catch (...) {
    }

    auto lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto &opt : GetHoistFunctionOptions()) {
        auto optLower = opt;
        std::transform(optLower.begin(), optLower.end(), optLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower == optLower)
            return opt;
    }

    return trimmed;
}

